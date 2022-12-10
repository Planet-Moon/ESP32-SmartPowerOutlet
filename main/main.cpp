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

#define USE_SSL 0

static const char *LOG_TAG = "main.cpp";

static const uint32_t BLINK_GPIO = 38; // LED pin

gpio_num_t RELAIS_PIN = GPIO_NUM_1;

LEDCtrl* myLed;

httpd_handle_t web_server = NULL;

void init_nvs() {
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

void init_sleep_mode(){
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

void init_wifi(){
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

void init_gpio(){
    gpio_reset_pin(RELAIS_PIN);
    gpio_set_direction(RELAIS_PIN, GPIO_MODE_OUTPUT);
    gpio_pullup_dis(RELAIS_PIN);
    gpio_pulldown_en(RELAIS_PIN);
    gpio_set_level(RELAIS_PIN, 0);
}

extern "C" void app_main(void) {
    myLed = new LEDCtrl(BLINK_GPIO, 1);
    init_nvs();
    init_gpio();
    init_sleep_mode();
    init_wifi();
    ESP_LOGW(LOG_TAG, "Hello, world!");
    myLed->setTo(0, 0, 0, 12);
    const wifi_ap_record_t* ap = nullptr;
    const bool enable_scan = true;
    if(enable_scan){
        std::shared_ptr<WifiScanner> scanner = std::make_shared<WifiScanner>();
        scanner->scan();
        ap = scanner->filterSSID(SSID);
        if (ap != nullptr) {
        myLed->setTo(0, 12, 3, 0);
        ESP_LOGI(LOG_TAG, "Found SSID");
        WifiScanner::logAp(ap);
        } else {
        myLed->setTo(0, 12, 0, 0);
        ESP_LOGI(LOG_TAG, "Could not find SSID");
        return;
        }
    }
    else{
        myLed->setTo(0, 12, 12, 12);
    }

    ESP_LOGI(LOG_TAG, "Try connecting to station");
    std::string ssid;
    if(ap == nullptr){
        ssid = SSID;
    }
    else{
        ESP_LOGI(LOG_TAG, "Use ssid from scan");
        ssid = reinterpret_cast<const char*>(ap->ssid);
    }

    #if USE_SSL
        https_register_callbacks(web_server);
        ESP_LOGW(LOG_TAG, "Using HTTPS");
    #else
        http_register_callbacks(web_server);
        ESP_LOGW(LOG_TAG, "Using HTTP");
    #endif

    auto station = std::make_shared<WifiStation>(ssid, PASSWORD);
    esp_err_t status = station->wifi_init_sta();
    if (status == 2)
    {
        myLed->setTo(0, 6, 0, 12);
        ESP_LOGE(LOG_TAG, "Failed to associate to AP, dying...");
        return;
    }

    ESP_LOGI(LOG_TAG, "Connected to AP, continue");

    int counter = 0;
    while(web_server == NULL){
        if(counter > 11){
            ESP_LOGE(LOG_TAG, "Webserver not initialized.");
            return;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ++counter;
    }

    ESP_ERROR_CHECK(register_uris(web_server));

    ESP_LOGI(LOG_TAG, "Init done. Waiting for requests...");
    ESP_ERROR_CHECK(esp_light_sleep_start());
}
