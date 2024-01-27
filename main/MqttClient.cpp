#include "MqttClient.h"
#include "esp_log.h"
#include <apps/esp_sntp.h>
#include "helpers.h"
#include <memory>
#include <stdio.h>
#include "authentication.h"

constexpr char TAG[] = "MqttClient";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

MqttClient::MqttClient()
{
// #ifdef CONFIG_BROKER_URL
//     std::string broker_url = CONFIG_BROKER_URL;
// #else
//     std::string broker_url = "192.168.178.107";
// #endif
    // std::string broker_uri = "ws://192.168.178.107:9002";
    // std::string broker_uri = MQTT_BROKER_URI;
    std::string broker_uri = "mqtt://broker.hivemq.com:1883";

    ESP_LOGI(TAG, "Configuration MqttClient");
    esp_mqtt_client_config_t mqtt_cfg;
    ESP_LOGI(TAG, "Set broker uri");
    mqtt_cfg.broker.address.uri = broker_uri.c_str();
    mqtt_cfg.credentials.client_id = NULL; // set client id based on mac address

    ESP_LOGI(TAG, "Init handle");
    _handle = esp_mqtt_client_init(&mqtt_cfg);
    if(_handle == NULL){
        ESP_LOGE(TAG, "MqttClient initialization failed");
        abort();
    }
    ESP_LOGI(TAG, "Register event handler");
    esp_mqtt_client_register_event(_handle, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "Start client");
    esp_mqtt_client_start(_handle);
    ESP_LOGI(TAG, "MqttClient Started");
}

MqttClient::~MqttClient()
{
    esp_mqtt_client_destroy(_handle);
}

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos, bool retain) const {
    timeval timestamp;
    if(gettimeofday(&timestamp, NULL) < 0){
        ESP_LOGE(TAG, "error reading time");
        return false;
    }
    unique_ptr_json json = unique_ptr_json(cJSON_CreateObject(), json_delete);
    cJSON_AddNumberToObject(json.get(), "timestamp", timestamp.tv_sec);
    cJSON_AddStringToObject(json.get(), "data", payload.c_str());
    ESP_LOGI(TAG, "publish payload");
    char* json_str = cJSON_PrintUnformatted(json.get());
    const bool res = esp_mqtt_client_publish(_handle, topic.c_str(), json_str, 0, qos, static_cast<int>(retain)) != -1;
    delete json_str;
    return res;
}
