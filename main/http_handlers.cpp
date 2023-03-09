#include "http_handlers.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>

const char *HTTP_TAG = "HTTP_LOG";

httpd_handle_t http_start_webserver()
{
    httpd_handle_t server = NULL;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 14;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(HTTP_TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        return server;
    }

    ESP_LOGI(HTTP_TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(HTTP_TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(HTTP_TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (server == NULL) {
        ESP_LOGI(HTTP_TAG, "Starting webserver");
        *server = http_start_webserver();
    }
    else{
        ESP_LOGW(HTTP_TAG, "Webserver already running?");
    }
}


// ESP_ERROR_CHECK(register_uris(web_server, myLed, &RELAIS_PIN));

// /* Event source task related definitions */
// ESP_EVENT_DEFINE_BASE(TASK_EVENTS);

void http_register_callbacks(httpd_handle_t& server){

    // esp_event_loop_args_t http_server_loop_args = {
    //     .queue_size = 5,
    //     .task_name = NULL
    // };

    // esp_event_loop_handle_t http_server_loop_handle;
    // ESP_ERROR_CHECK(esp_event_loop_create(&http_server_loop_args, &http_server_loop_handle));
    // ESP_ERROR_CHECK(esp_event_handler_instance_register_with(http_server_loop_handle, TASK_EVENTS, TASK_ITERATION_EVENT, task_iteration_handler, loop_with_task, NULL));

    // register callbacks
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
}
