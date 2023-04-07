#include "http_handlers.h"
#include "server_uri.h"
#include <esp_http_server.h>
#include "ledctrl.h"
#include <algorithm>
#include "esp_log.h"
#include "driver/gpio.h"
#include <memory>
#include "freertos/task.h"
#include "InternalTemperatureSensor.h"
#include "logger.h"
#include <string>
#ifdef CONFIG_SPO_NTC_ENABLE
#include "ntc.h"
#endif // CONFIG_SPO_NTC_ENABLE
#include <fft.h>

#define CONTENT_SIZE 2048
char content[CONTENT_SIZE];

static std::shared_ptr<LEDCtrl> myLed;
static const gpio_num_t* RELAIS_PIN;

struct Relais{
    bool state = false;
    bool init = false;
};

static Relais myRelais;

static const char* URI_TAG = "URI";

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

unique_ptr_json parse_request_json(httpd_req_t *req){
    ESP_LOGI(URI_TAG, "Receiving request with length %d", req->content_len);
    size_t recv_size = std::min(req->content_len, static_cast<size_t>(CONTENT_SIZE));
    ESP_LOGI(URI_TAG, "Reading %d bytes...", recv_size);
    memset(content, 0, CONTENT_SIZE);
    int ret = httpd_req_recv(req, content, recv_size);
    ESP_LOGI(URI_TAG, "bytes got: %d", ret);
    if(ret == 0){
        return unique_ptr_json(nullptr, json_delete);
    }
    ESP_LOGI(URI_TAG, "Got json: %s", content);
    auto req_json = unique_ptr_json(cJSON_ParseWithLength(content, recv_size), json_delete);
    if(!req_json){
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL){
            ESP_LOGE(URI_TAG, "Error parsing JSON %s", error_ptr);
        }
        return unique_ptr_json(nullptr, json_delete);
    }
    return req_json;
}

esp_err_t led_post_handler(httpd_req_t *req){
    unique_ptr_json req_json = parse_request_json(req);
    if(!req_json){
        return ESP_FAIL;
    }

    const LEDCtrl::LEDState led_state = myLed->getLedState(0);
    uint32_t r = led_state.r;
    uint32_t g = led_state.g;
    uint32_t b = led_state.b;
    bool on = led_state.on;

    const cJSON* color_red = cJSON_GetObjectItemCaseSensitive(req_json.get(), "red");
    if(color_red && cJSON_IsNumber(color_red)){
        int i = color_red->valueint;
        r = clamp(i, 0, 254);
    }
    else{
        ESP_LOGW(URI_TAG, "red not found or is not a number");
    }

    const cJSON* color_green = cJSON_GetObjectItemCaseSensitive(req_json.get(), "green");
    if(color_green && cJSON_IsNumber(color_green)){
        int i = color_green->valueint;
        g = clamp(i, 0, 254);
    }
    else{
        ESP_LOGW(URI_TAG, "green not found or is not a number");
    }

    const cJSON* color_blue = cJSON_GetObjectItemCaseSensitive(req_json.get(), "blue");
    if(color_blue && cJSON_IsNumber(color_blue)){
        int i = color_blue->valueint;
        b = clamp(i, 0, 254);
    }
    else{
        ESP_LOGW(URI_TAG, "blue not found or is not a number");
    }

    const cJSON* on_state = cJSON_GetObjectItemCaseSensitive(req_json.get(), "on");
    if(on_state && cJSON_IsBool(on_state)){
        on = on_state->valueint > 0;
    }
    else{
        ESP_LOGW(URI_TAG, "on not found or is not a boolean");
    }

    if(on){
        myLed->turnOn(0);
    }
    else{
        myLed->turnOff(0);
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
    .handler   = led_post_handler,
    .user_ctx = NULL
};

esp_err_t led_get_handler(httpd_req_t *req){
    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/json");

    auto json = unique_json(cJSON_CreateArray());

    const uint32_t n_led = myLed->getNumberOfLed();
    for(uint32_t i = 0; i < n_led; ++i){
        const LEDCtrl::LEDState led_state = myLed->getLedState(i);
        auto led = unique_json(cJSON_CreateObject());
        cJSON_AddNumberToObject(led.get(), "red", led_state.r);
        cJSON_AddNumberToObject(led.get(), "green", led_state.g);
        cJSON_AddNumberToObject(led.get(), "blue", led_state.b);
        cJSON_AddBoolToObject(led.get(), "on", led_state.on);
        cJSON_AddItemToArray(json.get(), led.release());
    }
    auto json_str = unique_char(cJSON_PrintUnformatted(json.get()));
    httpd_resp_send(req, json_str.get(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t led_get_uri = {
    .uri       = "/led",
    .method    = HTTP_GET,
    .handler   = led_get_handler,
    .user_ctx = NULL
};

esp_err_t relais_post_handler(httpd_req_t *req){
    unique_ptr_json req_json = parse_request_json(req);
    if(!req_json){
        return ESP_FAIL;
    }

    const cJSON* recv_relais_state = cJSON_GetObjectItemCaseSensitive(req_json.get(), "state");

    if(cJSON_IsBool(recv_relais_state)){
        bool req_state = recv_relais_state->valueint > 0;
        if(myRelais.state != req_state){
            gpio_set_level(*RELAIS_PIN, static_cast<int>(req_state));
            myRelais.state = req_state;
            myRelais.init = true;
        }
        else if(!myRelais.init){
            gpio_set_level(*RELAIS_PIN, req_state);
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
    .handler   = relais_post_handler,
    .user_ctx = NULL
};

esp_err_t relais_get_handler(httpd_req_t *req){
    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/json");

    auto json = unique_json(cJSON_CreateObject());
    cJSON_AddBoolToObject(json.get(), "state", myRelais.state);

    auto json_str = unique_char(cJSON_PrintUnformatted(json.get()));
    httpd_resp_send(req, json_str.get(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t relais_get_uri = {
    .uri       = "/relais",
    .method    = HTTP_GET,
    .handler   = relais_get_handler,
    .user_ctx = NULL
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
    .handler   = color_picker_handler,
    .user_ctx = NULL
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
    .handler   = flowbite_handler,
    .user_ctx = NULL
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
    .handler   = flowbite_map_handler,
    .user_ctx = NULL
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
    .handler   = index_handler,
    .user_ctx = NULL
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
    .handler   = relais_switch_handler,
    .user_ctx = NULL
};

extern const unsigned char led_switch_start[] asm("_binary_led_switch_js_gz_start");
extern const unsigned char led_switch_end[] asm("_binary_led_switch_js_gz_end");
esp_err_t led_switch_handler(httpd_req_t *req){
    const char* file = reinterpret_cast<const char*>(led_switch_start);
    size_t file_len = led_switch_end - led_switch_start;

    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req,"Content-Encoding", "gzip");
    httpd_resp_send(req, file, file_len);
    return ESP_OK;
}

const httpd_uri_t led_switch_uri = {
    .uri       = "/static/js/led_switch.js",
    .method    = HTTP_GET,
    .handler   = led_switch_handler,
    .user_ctx = NULL
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
    .handler   = style_handler,
    .user_ctx = NULL
};

// This example demonstrates how a human readable table of run time stats
// information is generated from raw data provided by uxTaskGetSystemState().
// The human readable table is written to pcWriteBuffer
unique_ptr_json vTaskGetRunTimeStats(){
    auto pcWriteBuffer = unique_ptr_json(cJSON_CreateObject(), json_delete);
    uint32_t ulTotalRunTime;
    float ulStatsAsPercentage;

    // Take a snapshot of the number of tasks in case it changes while this
    // function is executing.
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    cJSON_AddNumberToObject(pcWriteBuffer.get(), "number_of_tasks", uxArraySize);

    // Allocate a TaskStatus_t structure for each task.  An array could be
    // allocated statically at compile time.
    TaskStatus_t* pxTaskStatusArray = reinterpret_cast<TaskStatus_t*>(pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) ));

    if( pxTaskStatusArray != NULL )
    {
        // Generate raw status information about each task.
        uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime );

        // // For percentage calculations.
        // ulTotalRunTime /= 100UL;

        cJSON_AddNumberToObject(pcWriteBuffer.get(), "total_run_time", ulTotalRunTime);

        // For each populated position in the pxTaskStatusArray array,
        // format the raw data as human readable ASCII data
        for( UBaseType_t x = 0; x < uxArraySize; x++ )
        {
            auto task = unique_json(cJSON_CreateObject());

            // Avoid divide by zero errors.
            if( ulTotalRunTime > 0 ){
                // What percentage of the total run time has the task used?
                // This will always be rounded down to the nearest integer.
                // ulTotalRunTimeDiv100 has already been divided by 100.
                ulStatsAsPercentage = 100*pxTaskStatusArray[ x ].ulRunTimeCounter / static_cast<float>(ulTotalRunTime);
                cJSON_AddNumberToObject(task.get(), "run_time_counter_percent", ulStatsAsPercentage);
            }

            cJSON_AddNumberToObject(task.get(), "run_time_counter", pxTaskStatusArray[ x ].ulRunTimeCounter);
            cJSON_AddItemToObject(pcWriteBuffer.get(), pxTaskStatusArray[ x ].pcTaskName, task.release());
        }
    }

    // The array is no longer needed, free the memory it consumes.
    vPortFree( pxTaskStatusArray );

    return pcWriteBuffer;
}

std::string LUT(const esp_reset_reason_t& reason); // from main.cpp
esp_err_t system_status_handler(httpd_req_t *req){
    unique_ptr_json json = unique_ptr_json(cJSON_CreateObject(), json_delete);
    unique_ptr_json tasks_json = vTaskGetRunTimeStats();
    cJSON_AddItemToObject(json.get(), "tasks", tasks_json.release());

    cJSON_AddNumberToObject(json.get(), "free_heap_size", esp_get_free_heap_size());
    cJSON_AddNumberToObject(json.get(), "free_internal_heap_size", esp_get_free_internal_heap_size());
    cJSON_AddNumberToObject(json.get(), "minimum_free_heap_size", esp_get_minimum_free_heap_size());

    const esp_reset_reason_t reset_reason = esp_reset_reason();
    cJSON_AddNumberToObject(json.get(), "reset_reason", reset_reason);
    cJSON_AddStringToObject(json.get(), "reset_reason_str", LUT(reset_reason).c_str());

    InternalTemperatureSensor& temperatureSensor = InternalTemperatureSensor::getInstance();
    cJSON_AddNumberToObject(json.get(), "internal_temperature", temperatureSensor.readTemperature());

#ifdef CONFIG_SPO_NTC_ENABLE
    const float temperature = ntc::readTemperature();
    cJSON_AddNumberToObject(json.get(), "temperature", temperature);
#endif // CONFIG_SPO_NTC_ENABLE

    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/json");

    auto json_str = unique_char(cJSON_PrintUnformatted(json.get()));
    httpd_resp_send(req, json_str.get(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t system_status_uri = {
    .uri = "/system",
    .method = HTTP_GET,
    .handler = system_status_handler,
    .user_ctx = NULL
};

esp_err_t logger_handler(httpd_req_t *req){
    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/json");

    Logger& logger = Logger::getInstance();
    unique_ptr_json json = logger.getLog();
    auto json_str = unique_char(cJSON_PrintUnformatted(json.get()));
    httpd_resp_send(req, json_str.get(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t logger_uri = {
    .uri = "/logger",
    .method = HTTP_GET,
    .handler = logger_handler,
    .user_ctx = NULL
};

static long _counter = 0;
esp_err_t counter_handler(httpd_req_t *req){
    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/json");

    unique_ptr_json json = unique_ptr_json(cJSON_CreateObject(), json_delete);
    cJSON_AddNumberToObject(json.get(), "counter", _counter);
    auto json_str = unique_char(cJSON_PrintUnformatted(json.get()));
    httpd_resp_send(req, json_str.get(), HTTPD_RESP_USE_STRLEN);
    _counter++;
    if(_counter >= LONG_MAX){
        _counter = 0;
    }
    return ESP_OK;
}

const httpd_uri_t counter_uri = {
    .uri = "/counter",
    .method = HTTP_GET,
    .handler = counter_handler,
    .user_ctx = NULL
};

esp_err_t fft_handler(httpd_req_t *req){
    httpd_resp_set_status(req, "200");
    httpd_resp_set_type(req, "application/json");

    unique_ptr_json req_json = parse_request_json(req);
    if(!req_json)
        return ESP_FAIL;

    fft::Array* array = fft::fromJson(req_json.get());
    if(!array)
        return ESP_FAIL;

    fft::transform(*array);

    unique_ptr_json json = fft::toJson(*array);

    auto json_str = unique_char(cJSON_PrintUnformatted(json.get()));
    httpd_resp_send(req, json_str.get(), HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

const httpd_uri_t fft_uri = {
    .uri = "/fft",
    .method = HTTP_GET,
    .handler = fft_handler,
    .user_ctx = NULL
};

esp_err_t register_uris(httpd_handle_t& server, std::shared_ptr<LEDCtrl> _myLed, const gpio_num_t* _RELAIS_PIN){
    ESP_LOGI(URI_TAG, "Checking if server exists");
    if(server == NULL)
        return ESP_FAIL;

    myLed = _myLed;
    RELAIS_PIN = _RELAIS_PIN;

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
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &led_switch_uri));
    ESP_LOGI(URI_TAG, "led_switch.js get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &style_uri));
    ESP_LOGI(URI_TAG, "style.css get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &system_status_uri));
    ESP_LOGI(URI_TAG, "sytem_status get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &logger_uri));
    ESP_LOGI(URI_TAG, "logger get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &counter_uri));
    ESP_LOGI(URI_TAG, "counter get added");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &fft_uri));
    ESP_LOGI(URI_TAG, "fft get added");
    ESP_LOGI(URI_TAG, "Added all my uris");
    return ESP_OK;
}
