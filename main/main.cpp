#include "authentication.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "ledctrl.h"
#include "nvs_flash.h"
#include "wifiscan.h"
#include "wifistation.h"
#include <iostream>
#include <memory>
#include "esp_wifi.h"
#include "http_handlers.h"
#include "https_handlers.h"
#include "server_uri.h"
#include "driver/gpio.h"
#include "InternalTemperatureSensor.h"
#include <memory>
#include <apps/esp_sntp.h>
#include "logger.h"
// #include "MqttClient.h"
#include "helpers.h"

// enable HTTPS in config
// configUSE_TRACE_FACILITY
// enable temperature sensor in config
// change flashsize to 32 MB
// set partition size to single (large)

#define USE_SSL 0

static const char *LOG_TAG = "main.cpp";

static const uint32_t BLINK_GPIO = 38; // LED pin

static const gpio_num_t RELAIS_PIN = GPIO_NUM_1;

static std::shared_ptr<LEDCtrl> myLed;

static httpd_handle_t web_server = NULL;

static TaskHandle_t xHandle_wifiWatchdog = NULL;
static bool wifiWatchdog_run = true;

static void init_nvs() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_LOGI("init nvs", "flash erased");
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI("init nvs", "done");
}

static void init_sleep_mode(){
    ESP_ERROR_CHECK(esp_sleep_enable_wifi_wakeup());

    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO));
    // ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_AUTO));
    // ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_AUTO));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_AUTO));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_CPU, ESP_PD_OPTION_AUTO));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_AUTO));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_VDDSDIO, ESP_PD_OPTION_AUTO));

    esp_pm_config_esp32s3_t pm_config;
    pm_config.max_freq_mhz = 240;
    pm_config.min_freq_mhz = 20;
    pm_config.light_sleep_enable = false;
    // ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
}

static void init_wifi(){
    //initialize the esp network interface
    ESP_ERROR_CHECK(esp_netif_init());

    //initialize default esp event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //create wifi station in the wifi driver
    esp_netif_create_default_wifi_sta();

    //setup wifi station with the default wifi configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}

static void wifiWatchdog(void* pvParameters){
    ESP_LOGI("WifiWatchdog", "running");
    bool* run_watchdog = reinterpret_cast<bool*>(pvParameters);
    unsigned int task_high_watermark = 0;
    const char* watchdog_log = "WifiWatchdog";
    const TickType_t delay = 10'000 / portTICK_PERIOD_MS;
    Logger& logger = Logger::getInstance();
    uint32_t free_heap_size_last{0};
    while(*run_watchdog){

        // task watermark
        task_high_watermark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(watchdog_log, "task_high_watermark: %d", task_high_watermark);

#if false
        // free heap size
        const uint32_t free_heap_size = esp_get_free_heap_size();
        ESP_LOGI(watchdog_log, "free_heap_size: %ld", free_heap_size);
        if(free_heap_size_last > 0){
            ESP_LOGI(watchdog_log, "free_heap_size diff: %ld", free_heap_size-free_heap_size_last);
        }
        free_heap_size_last = free_heap_size;
#endif

        // delay
        vTaskDelay(delay);
    }
    ESP_LOGI(watchdog_log, "stopped");
}

static void create_wifi_watchdog(){
    const configSTACK_DEPTH_TYPE stack_size = 4096;
    xTaskCreatePinnedToCore(
        wifiWatchdog,
        "Wifi watchdog",
        stack_size,
        &wifiWatchdog_run,
        tskIDLE_PRIORITY,
        &xHandle_wifiWatchdog,
        0
    );
    if(xHandle_wifiWatchdog){
        ESP_LOGI("Wifi watchdog", "Task created successfully");
    }
    else{
        ESP_LOGE("Wifi watchdog", "Task creation failed");
    }
    configASSERT( xHandle_wifiWatchdog );

    // if( xHandle_wifiWatchdog != NULL )
    // {
    //     vTaskDelete( xHandle_wifiWatchdog );
    // }
}

static void init_gpio(){
    gpio_reset_pin(RELAIS_PIN);
    gpio_set_direction(RELAIS_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(RELAIS_PIN);
    gpio_pulldown_en(RELAIS_PIN);
    gpio_set_level(RELAIS_PIN, 0);
}

static bool scan_wifi(const std::string& ssid){
    std::shared_ptr<WifiScanner> scanner = std::make_shared<WifiScanner>();
    scanner->scan();
    const wifi_ap_record_t* ap = scanner->filterSSID(ssid);
    if (ap != nullptr) {
        myLed->setTo(0, 12, 3, 0);
        WifiScanner::logAp(ap);
    } else {
        myLed->setTo(0, 12, 0, 0);
    }
    return ap != nullptr;
}

void init_SNTP(){
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    int sntp_status = sntp_get_sync_status();
}

void test_json(){
    const uint32_t free_before = esp_get_free_heap_size();
    {
        auto json = unique_ptr_json(cJSON_CreateObject(), json_delete);
        // cJSON* json = cJSON_CreateObject();
        {
            auto obj = unique_ptr_json(cJSON_CreateObject(), json_delete);
            cJSON_AddNumberToObject(obj.get(), "number", 3.14156);
            InternalTemperatureSensor& temperatureSensor = InternalTemperatureSensor::getInstance();
            const auto temp = temperatureSensor.readTemperature();
            cJSON_AddNumberToObject(obj.get(), "temp", temp);
            cJSON_AddStringToObject(obj.get(), "message", "hello world");
            cJSON* obj_ptr = obj.release();
            cJSON_AddItemToObject(json.get(), "obj", obj_ptr);
        }
        {
            char* buffer = cJSON_PrintUnformatted(json.get());
            // cJSON_Delete(json);
            printf("test json: %s\n", buffer);
            delete buffer;
        }
        {
            cJSON* test_read = cJSON_GetObjectItemCaseSensitive(json.get(), "obj");
            char* buffer = cJSON_PrintUnformatted(test_read);
            printf("test read: %s\n", buffer);
            delete buffer;
        }
    }
    const uint32_t free_after = esp_get_free_heap_size();
    printf("memory difference: %ld\n", free_after - free_before);
    printf("heap memory: %ld\n\n", free_after);
}

std::string LUT(const esp_reset_reason_t& reason){
    switch(reason) {
    default:
    case ESP_RST_UNKNOWN:
        return "Reset reason can not be determined";
    case ESP_RST_POWERON:
        return "Reset due to power-on event";
    case ESP_RST_EXT:
        return "Reset by external pin (not applicable for ESP32)";
    case ESP_RST_SW:
        return "Software reset via esp_restart";
    case ESP_RST_PANIC:
        return "Software reset due to exception/panic";
    case ESP_RST_INT_WDT:
        return "Reset (software or hardware) due to interrupt watchdog";
    case ESP_RST_TASK_WDT:
        return "Reset due to task watchdog";
    case ESP_RST_WDT:
        return "Reset due to other watchdogs";
    case ESP_RST_DEEPSLEEP:
        return "Reset after exiting deep sleep mode";
    case ESP_RST_BROWNOUT:
        return "Brownout reset (software or hardware)";
    case ESP_RST_SDIO:
        return "Reset over SDIO";
    }
}

extern "C" void app_main(void) {
    ESP_LOGI(LOG_TAG, "[APP] Startup..");
    ESP_LOGI(LOG_TAG, "[APP] Free memory: %ld bytes", esp_get_free_heap_size());
    ESP_LOGI(LOG_TAG, "[APP] IDF version: %s", esp_get_idf_version());
    const esp_reset_reason_t reset_reason{esp_reset_reason()};
    ESP_LOGI(LOG_TAG, "[APP] esp_reset_reason: %s", LUT(reset_reason).c_str());

    esp_log_level_set("wifi", ESP_LOG_VERBOSE);

    myLed = std::make_shared<LEDCtrl>(BLINK_GPIO, 1);
    init_nvs();
    init_gpio();
    init_sleep_mode();
    init_wifi();
    vTaskDelay(500 / portTICK_PERIOD_MS);

    ESP_LOGW(LOG_TAG, "Hello, world!");
    myLed->setTo(0, 0, 0, 12);

    ESP_LOGI(LOG_TAG, "Try connecting to station");
    const bool ap_found = scan_wifi(SSID); // scanning
    if(ap_found){
        ESP_LOGI(LOG_TAG, "Found SSID");
        ESP_LOGI(LOG_TAG, "Use ssid from scan");
    }
    else{
        ESP_LOGE(LOG_TAG, "Could not find SSID");
        return;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);

    #if USE_SSL
        https_register_callbacks(web_server);
        ESP_LOGW(LOG_TAG, "Using HTTPS");
    #else
        http_register_callbacks(web_server);
        ESP_LOGW(LOG_TAG, "Using HTTP");
    #endif
    vTaskDelay(500 / portTICK_PERIOD_MS);

    {
        auto station = std::make_shared<WifiStation>(SSID, PASSWORD);
        esp_err_t status = station->wifi_init_sta(myLed);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        if (status == 2)
        {
            myLed->setTo(0, 6, 0, 12);
            ESP_LOGE(LOG_TAG, "Failed to associate to AP, dying...");
            return;
        }
    }
    ESP_LOGI(LOG_TAG, "Connected to AP, continue");
    init_SNTP();

    web_server = http_start_webserver();
    if(web_server == NULL){
        ESP_LOGE(LOG_TAG, "Web server start failed");
            return;
        }
    ESP_ERROR_CHECK(register_uris(web_server, myLed, &RELAIS_PIN));

    create_wifi_watchdog();

    ESP_LOGI(LOG_TAG, "Init done. Waiting for requests...");
    // ESP_ERROR_CHECK(esp_light_sleep_start());

    InternalTemperatureSensor& temperatureSensor = InternalTemperatureSensor::getInstance();

    Logger& logger = Logger::getInstance();

    logger.addLog("app", "Init done");
    vTaskDelay(100 / portTICK_PERIOD_MS);
    std::string reset_log = "Reset reason: "+std::to_string(reset_reason);
    reset_log += " ("+LUT(reset_reason)+")";
    logger.addLog("app", reset_log);

    // MqttClient& mqttClient = MqttClient::getInstance();
    // mqttClient.publish("/device/SmartPowerOutlet/1/app", "Init done", 1, false);

    unsigned int task_high_watermark = 0;
    float temp = 0.f;
    while (true){
        task_high_watermark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(LOG_TAG, "main_task_high_watermark: %d", task_high_watermark);
        temp = temperatureSensor.readTemperature();
        ESP_LOGI(LOG_TAG, "Internal temperature: %.2f °C", temp);
        vTaskDelay(20'000 / portTICK_PERIOD_MS);
    }

}
