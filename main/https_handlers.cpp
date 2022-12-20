#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"

#include "esp_https_server.h"
#include "esp_tls.h"
#include "sdkconfig.h"
#include "http_handlers.h"

// #define CONFIG_EXAMPLE_ENABLE_HTTPS_USER_CALLBACK 1
// #define CONFIG_ESP_TLS_USING_MBEDTLS 1


const char *HTTPS_TAG = "HTTPS_LOG";

/* An HTTP GET handler */
esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h1>Hello Secure World!</h1>", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

#if CONFIG_EXAMPLE_ENABLE_HTTPS_USER_CALLBACK
#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
void print_peer_cert_info(const mbedtls_ssl_context *ssl)
{
    const mbedtls_x509_crt *cert;
    const size_t buf_size = 1024;
    char *buf = static_cast<char*>(calloc(buf_size, sizeof(char)));
    if (buf == NULL) {
        ESP_LOGE(HTTPS_TAG, "Out of memory - Callback execution failed!");
        return;
    }

    // Logging the peer certificate info
    cert = mbedtls_ssl_get_peer_cert(ssl);
    if (cert != NULL) {
        mbedtls_x509_crt_info((char *) buf, buf_size - 1, "    ", cert);
        ESP_LOGI(HTTPS_TAG, "Peer certificate info:\n%s", buf);
    } else {
        ESP_LOGW(HTTPS_TAG, "Could not obtain the peer certificate!");
    }

    free(buf);
}
#endif
/**
 * Example callback function to get the certificate of connected clients,
 * whenever a new SSL connection is created and closed
 *
 * Can also be used to other information like Socket FD, Connection state, etc.
 *
 * NOTE: This callback will not be able to obtain the client certificate if the
 * following config `Set minimum Certificate Verification mode to Optional` is
 * not enabled (enabled by default in this example).
 *
 * The config option is found here - Component config → ESP-TLS
 *
 */
void https_server_user_callback(esp_https_server_user_cb_arg_t *user_cb)
{
    ESP_LOGI(HTTPS_TAG, "User callback invoked!");
#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
    mbedtls_ssl_context *ssl_ctx = NULL;
#endif
    if(user_cb->user_cb_state == httpd_ssl_user_cb_state_t::HTTPD_SSL_USER_CB_SESS_CREATE){
        ESP_LOGD(HTTPS_TAG, "At session creation");

        // Logging the socket FD
        int sockfd = -1;
        esp_err_t esp_ret;
        esp_ret = esp_tls_get_conn_sockfd(user_cb->tls, &sockfd);
        if (esp_ret != ESP_OK) {
            ESP_LOGE(HTTPS_TAG, "Error in obtaining the sockfd from tls context");
            return;
        }
        ESP_LOGI(HTTPS_TAG, "Socket FD: %d", sockfd);
#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
        ssl_ctx = (mbedtls_ssl_context *) esp_tls_get_ssl_context(user_cb->tls);
        if (ssl_ctx == NULL) {
            ESP_LOGE(HTTPS_TAG, "Error in obtaining ssl context");
            return;
        }
        // Logging the current ciphersuite
        ESP_LOGI(HTTPS_TAG, "Current Ciphersuite: %s", mbedtls_ssl_get_ciphersuite(ssl_ctx));
#endif
    }
    else if(user_cb->user_cb_state == httpd_ssl_user_cb_state_t::HTTPD_SSL_USER_CB_SESS_CLOSE){
        ESP_LOGD(HTTPS_TAG, "At session close");
#ifdef CONFIG_ESP_TLS_USING_MBEDTLS
        // Logging the peer certificate
        ssl_ctx = (mbedtls_ssl_context *) esp_tls_get_ssl_context(user_cb->tls);
        if (ssl_ctx == NULL) {
            ESP_LOGE(HTTPS_TAG, "Error in obtaining ssl context");
            return;
        }
        print_peer_cert_info(ssl_ctx);
#endif
    }
    else{
        ESP_LOGE(HTTPS_TAG, "Illegal state!");
        return;
    }
}
#endif

httpd_handle_t start_webserver()
{
    httpd_handle_t server = NULL;

    // Start the httpd server
    ESP_LOGI(HTTPS_TAG, "Starting server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();

    extern const unsigned char servercert_start[] asm("_binary_servercert_pem_start");
    extern const unsigned char servercert_end[]   asm("_binary_servercert_pem_end");
    conf.servercert = servercert_start;
    conf.servercert_len = servercert_end - servercert_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

#if CONFIG_EXAMPLE_ENABLE_HTTPS_USER_CALLBACK
    conf.user_cb = https_server_user_callback;
#endif
    esp_err_t ret = httpd_ssl_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGI(HTTPS_TAG, "Error starting server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(HTTPS_TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &root);
    return server;
}

esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_ssl_stop(server);
}

void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(HTTPS_TAG, "Failed to stop https server");
        }
    }
}

void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        *server = start_webserver();
    }
    else{
        ESP_LOGW(HTTPS_TAG, "Webserver already running?");
    }
}

void https_register_callbacks(httpd_handle_t& server){
    // register callbacks
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
}
