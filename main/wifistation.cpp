#include "wifistation.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "ledctrl.h"
#include "logger.h"


/** DEFINES **/
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_SUCCESS WIFI_CONNECTED_BIT
#define WIFI_FAILURE WIFI_FAIL_BIT
#define TCP_SUCCESS WIFI_CONNECTED_BIT
#define TCP_FAILURE WIFI_FAIL_BIT
#define MAX_FAILURES 10

/** GLOBALS **/

static LEDCtrl* myLed = nullptr;

// event group to contain status information
static EventGroupHandle_t wifi_event_group;

// retry tracker
static uint32_t _max_retries = 10;
static int s_retry_num = 0;

static bool _wifi_connected = false;
static bool _wifi_connected_old = false;

// task tag
constexpr char TAG[] = "wifi station";

/** FUNCTIONS **/

void check_wifi_error(const esp_err_t& err){
    switch(err){
        case ESP_ERR_WIFI_NOT_INIT:
            ESP_LOGE(TAG, "WiFi is not initialized by esp_wifi_init");
            break;
        case ESP_ERR_WIFI_NOT_STARTED:
            ESP_LOGE(TAG, "WiFi is not started by esp_wifi_start");
            break;
        case ESP_ERR_WIFI_CONN:
            ESP_LOGE(TAG, "WiFi internal error, station or soft-AP control block wrong");
            break;
        case ESP_ERR_WIFI_SSID:
            ESP_LOGE(TAG, "SSID of AP which station connects is invalid ");
            break;
        case ESP_OK:
            ESP_LOGI(TAG, "connect call succeed");
            break;
        default:
            ESP_LOGW(TAG, "uncovered error");
            break;
    }
    ESP_ERROR_CHECK(err);
}

WifiStation::WifiStation(const std::string &ssid, const std::string &password)
    : _ssid(ssid), _password(password) {}

WifiStation::~WifiStation() {}

//event handler for wifi events
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    Logger& logger = Logger::getInstance();
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to AP...");
        esp_err_t err = esp_wifi_connect();
        ESP_ERROR_CHECK(err);
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED){
        _wifi_connected = true;
        s_retry_num = 0;
        _max_retries = 1000;
        logger.addLog("WiFi", "Connected");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        _wifi_connected = false;
        if (s_retry_num < _max_retries) {
            logger.addLog("WiFi", "Disconnected - Retrying. Try "+std::to_string(s_retry_num)+"/"+std::to_string(_max_retries));
            ESP_LOGI(TAG, "retry to connect to the AP");
            esp_err_t err = esp_wifi_connect();
            ESP_ERROR_CHECK(err);
            s_retry_num++;
        } else {
            logger.addLog("WiFi", "Disconnected - Retry failed");
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }

    if(_wifi_connected == false && _wifi_connected_old == true){
        myLed->setTo(0,5,0,0); // red
    }
    else if(_wifi_connected == true && _wifi_connected_old == false){
        myLed->setTo(0, 3, 5, 0);
    }
    _wifi_connected_old = _wifi_connected;
}

//event handler for ip events
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    Logger& logger = Logger::getInstance();
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        myLed->setTo(0, 0, 5, 0);
        logger.addLog("Ip", "Got Ip");
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP){
        ESP_LOGW(TAG, "lost ip");
        myLed->setTo(0, 5, 0, 6);
        logger.addLog("Ip", "Lost Ip");
    }
}

esp_err_t WifiStation::wifi_init_sta(LEDCtrl* _myLed) {
    myLed = _myLed;
    int status = WIFI_FAILURE;

    /** EVENT LOOP CRAZINESS **/
    wifi_event_group = xEventGroupCreate();

    esp_err_t res;

    esp_event_handler_instance_t wifi_handler_event_instance;
    res = esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        &wifi_handler_event_instance);
    ESP_ERROR_CHECK(res);

    esp_event_handler_instance_t got_ip_event_instance;
    res = esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &ip_event_handler,
        NULL,
        &got_ip_event_instance);
    ESP_ERROR_CHECK(res);

    { // check size of ssid and password before copy
        bool _error = false;
        if(_ssid.size() >= 32){
            ESP_LOGE(TAG, "ssid too long");
            _error = true;
        }
        if(_password.size() >= 64){
            ESP_LOGE(TAG, "password too long");
            _error = true;
        }
        if(_error)
            return ESP_FAIL;
    }
    wifi_config_t wifi_config;
    memset(wifi_config.sta.ssid, 0, sizeof(wifi_config.sta.ssid));
    memset(wifi_config.sta.password, 0, sizeof(wifi_config.sta.password));
    strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), _ssid.c_str());
    strcpy(reinterpret_cast<char*>(wifi_config.sta.password), _password.c_str());
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN; // WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.channel = 0; // unknown = 0
    wifi_config.sta.pmf_cfg.capable = true; // deprecated
    wifi_config.sta.pmf_cfg.required = false;
    wifi_config.sta.bssid_set = false; // generally set to false
    wifi_config.sta.listen_interval = 3; // 0 -> 3 as default

    // set the wifi controller to be a station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // set the wifi config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    wifi_ps_type_t ps_type = wifi_ps_type_t::WIFI_PS_MAX_MODEM;
    ESP_ERROR_CHECK(esp_wifi_get_ps(&ps_type));
    ESP_LOGW(TAG, "ESP WiFi power mode: %d", ps_type);

    // start the wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta initialization complete. Waiting for connection...");
    ESP_LOGI(TAG, "Try connecting to ap SSID: \"%s\" password: \"%s\"", wifi_config.sta.ssid, wifi_config.sta.password);

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
    * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
    * bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                            pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we
    * can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID: \"%s\" password: \"%s\"", _ssid.c_str(), _password.c_str());
        status = WIFI_CONNECTED_BIT;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID: \"%s\", password: \"%s\"", _ssid.c_str(), _password.c_str());
        status = WIFI_FAIL_BIT;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        status = WIFI_FAIL_BIT;
    }

    // /* The event will not be processed after unregister */
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
    // vEventGroupDelete(wifi_event_group);

    // esp_event_handler_instance_t wifi_connected_handler_event_instance;
    // res = esp_event_handler_instance_register(WIFI_EVENT,
    //     ESP_EVENT_ANY_ID,
    //     &wifi_connected_event_handler,
    //     NULL,
    //     &wifi_connected_handler_event_instance);
    // ESP_ERROR_CHECK(res);

    return status;
}
