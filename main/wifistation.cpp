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

extern LEDCtrl* myLed;

// event group to contain status information
static EventGroupHandle_t wifi_event_group;

// retry tracker
static uint32_t _max_retries = 5;
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
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Connecting to AP...");
        esp_err_t err = (esp_wifi_connect());
        ESP_ERROR_CHECK(err);
    }
    else if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED){
        _wifi_connected = true;
        _max_retries = 0;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        _wifi_connected = false;
        if (s_retry_num < _max_retries) {
            ESP_LOGI(TAG, "retry to connect to the AP");
            esp_err_t err = esp_wifi_connect();
            ESP_ERROR_CHECK(err);
            s_retry_num++;
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }

    if(_wifi_connected == false && _wifi_connected_old == true){
        myLed->setTo(0,5,0,0); // red
    }
    else if(_wifi_connected == true && _wifi_connected_old == false){
        myLed->setTo(0, 5, 12, 0);
    }
    _wifi_connected_old = _wifi_connected;
}

//event handler for ip events
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        myLed->setTo(0, 0, 12, 0);
    }
    else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP){
        ESP_LOGW(TAG, "lost ip");
        myLed->setTo(0, 5, 0, 6);
    }
}

esp_err_t WifiStation::wifi_init_sta(void) {
    int status = WIFI_FAILURE;

    /** EVENT LOOP CRAZINESS **/
    wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t wifi_handler_event_instance;

    esp_err_t res;
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

    wifi_config_t wifi_config;
    strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), _ssid.c_str());
    strcpy(reinterpret_cast<char*>(wifi_config.sta.password), _password.c_str());
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.channel = 0; // unknown = 0
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    wifi_config.sta.bssid_set = false;
    wifi_config.sta.listen_interval = 3;

    // set the wifi controller to be a station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // set the wifi config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    // start the wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    wifi_ps_type_t ps_type = wifi_ps_type_t::WIFI_PS_MAX_MODEM;
    ESP_ERROR_CHECK(esp_wifi_get_ps(&ps_type));
    ESP_LOGW(TAG, "ESP WiFi power mode: %d", ps_type);

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

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
    vEventGroupDelete(wifi_event_group);

    return status;
}


// connect to the server and return the result
esp_err_t connect_tcp_server(void)
{
	struct sockaddr_in serverInfo = {0};
	char readBuffer[1024] = {0};

	serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = 0x8DB2A8C0;
	serverInfo.sin_port = htons(8885);


	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		ESP_LOGE(TAG, "Failed to create a socket..?");
		return TCP_FAILURE;
	}


	if (connect(sock, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) != 0)
	{
		ESP_LOGE(TAG, "Failed to connect to %s!", inet_ntoa(serverInfo.sin_addr.s_addr));
		close(sock);
		return TCP_FAILURE;
	}

	ESP_LOGI(TAG, "Connected to TCP server.");
	bzero(readBuffer, sizeof(readBuffer));
    int r = read(sock, readBuffer, sizeof(readBuffer)-1);
    std::string recv = "";
    for(int i = 0; i < r; i++) {
        putchar(readBuffer[i]);
        recv += readBuffer[i];
    }
    ESP_LOGI(TAG, "Received: %s", recv.c_str());

    if (strcmp(readBuffer, "HELLO") == 0)
    {
    	ESP_LOGI(TAG, "WE DID IT!");
    }

    return TCP_SUCCESS;
}
