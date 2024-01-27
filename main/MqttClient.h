#pragma once
#include "mqtt_client.h"
#include <string>

class MqttClient{
private:
    MqttClient();
    ~MqttClient();

    esp_mqtt_client_handle_t _handle = nullptr;

public:
    MqttClient(const MqttClient&) = delete;
    MqttClient& operator = (const MqttClient&) = delete;

    bool publish(const std::string& topic, const std::string& payload, int qos, bool retain) const;

    static MqttClient& getInstance(){
        static MqttClient instance;        // (1)
        return instance;
    }
};
