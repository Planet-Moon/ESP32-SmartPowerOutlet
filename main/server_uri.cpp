#include "http_handlers.h"
#include "server_uri.h"
#include <esp_http_server.h>
#include "ledctrl.h"
#include "cJSON.h"
#include <algorithm>
#include "esp_log.h"
#include "driver/gpio.h"
#include <memory>

#define CONTENT_SIZE 512
char content[CONTENT_SIZE];

extern LEDCtrl* myLed;
extern gpio_num_t RELAIS_PIN;

struct Relais{
    bool state = false;
    bool init = false;
};

Relais myRelais;


const char* URI_TAG = "URI";

#if FALSE
esp_err_t post_handler(httpd_req_t *req)
{
    /* Destination buffer for content of HTTP POST request.
     * httpd_req_recv() accepts char* only, but content could
     * as well be any binary data (needs type casting).
     * In case of string data, null termination will be absent, and
     * content length would give length of string */

    /* Truncate if content length larger than the buffer */
    size_t recv_size = std::min(req->content_len, CONTENT_SIZE);

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            /* In case of timeout one can choose to retry calling
             * httpd_req_recv(), but to keep it simple, here we
             * respond with an HTTP 408 (Request Timeout) error */
            httpd_resp_send_408(req);
        }
        /* In case of error, returning ESP_FAIL will
         * ensure that the underlying socket is closed */
        return ESP_FAIL;
    }

    /* Send a simple response */
    const char resp[] = "URI POST Response";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
#endif

int clamp(int val, int min, int max){
    if(val < min)
        return min;
    if(val > max)
        return max;
    return val;
}

std::unique_ptr<cJSON> parse_request_json(httpd_req_t *req){
    ESP_LOGI(URI_TAG, "Receiving request with length %d", req->content_len);
    size_t recv_size = std::min(req->content_len, static_cast<size_t>(CONTENT_SIZE));
    ESP_LOGI(URI_TAG, "Reading %d bytes...", recv_size);
    memset(content, 0, CONTENT_SIZE);
    int ret = httpd_req_recv(req, content, recv_size);
    ESP_LOGI(URI_TAG, "bytes got: %d", ret);
    if(ret == 0){
        return {};
    }
    ESP_LOGI(URI_TAG, "Got json: %s", content);
    std::unique_ptr<cJSON> req_json = std::unique_ptr<cJSON>(cJSON_ParseWithLength(content, recv_size));
    if(!req_json){
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL){
            ESP_LOGE(URI_TAG, "Error parsing JSON %s", error_ptr);
        }
        return {};
    }
    return req_json;
}

esp_err_t led_post_handler(httpd_req_t *req){
    auto req_json = parse_request_json(req);
    if(!req_json){
        return ESP_FAIL;
    }

    uint32_t r = 0;
    uint32_t g = 0;
    uint32_t b = 0;

    const cJSON* color_red = NULL;
    const cJSON* color_green = NULL;
    const cJSON* color_blue = NULL;

    color_red = cJSON_GetObjectItemCaseSensitive(req_json.get(), "red");
    if(cJSON_IsNumber(color_red)){
        int i = color_red->valueint;
        r = clamp(i, 0, 254);
    }
    else{
        ESP_LOGW(URI_TAG, "red is not a number");
    }

    color_green = cJSON_GetObjectItemCaseSensitive(req_json.get(), "green");
    if(cJSON_IsNumber(color_green)){
        int i = color_green->valueint;
        g = clamp(i, 0, 254);
    }
    else{
        ESP_LOGW(URI_TAG, "green is not a number");
    }

    color_blue = cJSON_GetObjectItemCaseSensitive(req_json.get(), "blue");
    if(cJSON_IsNumber(color_blue)){
        int i = color_blue->valueint;
        b = clamp(i, 0, 254);
    }
    else{
        ESP_LOGW(URI_TAG, "blue is not a number");
    }

    myLed->setTo(0,r,g,b);

    httpd_resp_set_status(req, "200");
    // httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t led_post_uri = {
    .uri       = "/led",
    .method    = HTTP_POST,
    .handler   = led_post_handler
};

esp_err_t led_get_handler(httpd_req_t *req){
    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/json");

    cJSON* json = cJSON_CreateArray();

    const uint32_t n_led = myLed->getNumberOfLed();
    for(uint32_t i = 0; i < n_led; ++i){
        std::array<uint32_t, 3> led_state = myLed->getLedColor(i);
        cJSON* led = cJSON_CreateObject();
        cJSON_AddNumberToObject(led, "red", led_state[0]);
        cJSON_AddNumberToObject(led, "green", led_state[1]);
        cJSON_AddNumberToObject(led, "blue", led_state[2]);
        cJSON_AddItemToArray(json,led);
    }

    httpd_resp_send(req, cJSON_PrintUnformatted(json), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t led_get_uri = {
    .uri       = "/led",
    .method    = HTTP_GET,
    .handler   = led_get_handler
};

esp_err_t relais_post_handler(httpd_req_t *req){
    std::unique_ptr<cJSON> req_json = parse_request_json(req);
    if(!req_json){
        return ESP_FAIL;
    }

    cJSON* recv_relais_state = cJSON_GetObjectItemCaseSensitive(req_json.get(), "state");

    if(cJSON_IsBool(recv_relais_state)){
        bool req_state = recv_relais_state->valueint > 0;
        if(myRelais.state != req_state){
            gpio_set_level(RELAIS_PIN, static_cast<int>(req_state));
            myRelais.state = req_state;
            myRelais.init = true;
        }
        else if(!myRelais.init){
            gpio_set_level(RELAIS_PIN, req_state);
            myRelais.state = req_state;
            myRelais.init = true;
        }
    }
    else{
        ESP_LOGW(URI_TAG, "state is not bool");
    }

    httpd_resp_set_status(req, "200");
    // httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t relais_post_uri = {
    .uri       = "/relais",
    .method    = HTTP_POST,
    .handler   = relais_post_handler
};

esp_err_t relais_get_handler(httpd_req_t *req){
    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/json");

    cJSON* json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "state", myRelais.state);

    httpd_resp_send(req, cJSON_PrintUnformatted(json), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t relais_get_uri = {
    .uri       = "/relais",
    .method    = HTTP_GET,
    .handler   = relais_get_handler
};

extern const unsigned char color_picker_start[] asm("_binary_color_picker_js_gz_start");
extern const unsigned char color_picker_end[] asm("_binary_color_picker_js_gz_end");
esp_err_t color_picker_handler(httpd_req_t *req){
    const char* file = reinterpret_cast<const char*>(color_picker_start);
    size_t file_len = color_picker_end - color_picker_start;

    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req,"Content-Encoding", "gzip");
    httpd_resp_send(req, file, file_len);
    return ESP_OK;
}

const httpd_uri_t color_picker_uri = {
    .uri       = "/static/js/color_picker.js",
    .method    = HTTP_GET,
    .handler   = color_picker_handler
};

extern const unsigned char flowbite_js_start[] asm("_binary_flowbite_js_gz_start");
extern const unsigned char flowbite_js_end[] asm("_binary_flowbite_js_gz_end");
esp_err_t flowbite_handler(httpd_req_t *req){
    const char* file = reinterpret_cast<const char*>(flowbite_js_start);
    size_t file_len = flowbite_js_end - flowbite_js_start;

    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req,"Content-Encoding", "gzip");
    httpd_resp_send(req, file, file_len);
    return ESP_OK;
}

const httpd_uri_t flowbite_uri = {
    .uri       = "/static/lib/flowbite.js",
    .method    = HTTP_GET,
    .handler   = flowbite_handler
};

extern const unsigned char flowbite_map_start[] asm("_binary_flowbite_js_map_gz_start");
extern const unsigned char flowbite_map_end[] asm("_binary_flowbite_js_map_gz_end");
esp_err_t flowbite_map_handler(httpd_req_t *req){
    const char* file = reinterpret_cast<const char*>(flowbite_map_start);
    size_t file_len = flowbite_map_end - flowbite_map_start;

    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req,"Content-Encoding", "gzip");
    httpd_resp_send(req, file, file_len);
    return ESP_OK;
}

const httpd_uri_t flowbite_map_uri = {
    .uri       = "/static/lib/flowbite.js.map",
    .method    = HTTP_GET,
    .handler   = flowbite_map_handler
};

extern const unsigned char index_start[] asm("_binary_index_html_gz_start");
extern const unsigned char index_end[] asm("_binary_index_html_gz_end");
esp_err_t index_handler(httpd_req_t *req){
    const char* file = reinterpret_cast<const char*>(index_start);
    size_t file_len = index_end - index_start;

    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req,"Content-Encoding", "gzip");
    httpd_resp_send(req, file, file_len);
    return ESP_OK;
}

const httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler
};

extern const unsigned char relais_switch_start[] asm("_binary_relais_switch_js_gz_start");
extern const unsigned char relais_switch_end[] asm("_binary_relais_switch_js_gz_end");
esp_err_t relais_switch_handler(httpd_req_t *req){
    const char* file = reinterpret_cast<const char*>(relais_switch_start);
    size_t file_len = relais_switch_end - relais_switch_start;

    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req,"Content-Encoding", "gzip");
    httpd_resp_send(req, file, file_len);
    return ESP_OK;
}

const httpd_uri_t relais_switch_uri = {
    .uri       = "/static/js/relais_switch.js",
    .method    = HTTP_GET,
    .handler   = relais_switch_handler
};

extern const unsigned char style_start[] asm("_binary_style_css_gz_start");
extern const unsigned char style_end[] asm("_binary_style_css_gz_end");
esp_err_t style_handler(httpd_req_t *req){
    const char* file = reinterpret_cast<const char*>(style_start);
    size_t file_len = style_end - style_start;

    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req,"Content-Encoding", "gzip");
    httpd_resp_send(req, file, file_len);
    return ESP_OK;
}

const httpd_uri_t style_uri = {
    .uri       = "/static/css/style.css",
    .method    = HTTP_GET,
    .handler   = style_handler
};

esp_err_t register_uris(httpd_handle_t& server){
    ESP_LOGI(URI_TAG, "Checking if server exists");
    if(server == NULL)
        return ESP_FAIL;
    memset(content, 0, CONTENT_SIZE);

    ESP_LOGI(URI_TAG, "Begin adding uris");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &led_post_uri));
    ESP_LOGI(URI_TAG, "led post added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &led_get_uri));
    ESP_LOGI(URI_TAG, "led get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &relais_post_uri));
    ESP_LOGI(URI_TAG, "relais post added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &relais_get_uri));
    ESP_LOGI(URI_TAG, "relais get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &color_picker_uri));
    ESP_LOGI(URI_TAG, "color_picker.js get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &flowbite_uri));
    ESP_LOGI(URI_TAG, "flowbite.js get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &flowbite_map_uri));
    ESP_LOGI(URI_TAG, "flowbite.js.map get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &index_uri));
    ESP_LOGI(URI_TAG, "index.html get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &relais_switch_uri));
    ESP_LOGI(URI_TAG, "relais_switch.js get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &style_uri));
    ESP_LOGI(URI_TAG, "style.css get added");
    ESP_LOGI(URI_TAG, "Added all my uris");
    return ESP_OK;
}
