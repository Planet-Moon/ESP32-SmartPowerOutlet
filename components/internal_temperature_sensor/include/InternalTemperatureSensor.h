#pragma once
#include "driver/temperature_sensor.h"

class InternalTemperatureSensor{
private:
    InternalTemperatureSensor();
    ~InternalTemperatureSensor();

    temperature_sensor_handle_t temp_handle = NULL;

public:
    InternalTemperatureSensor(const InternalTemperatureSensor&) = delete;
    InternalTemperatureSensor& operator = (const InternalTemperatureSensor&) = delete;

    float readTemperature() const;

    static InternalTemperatureSensor& getInstance(){
        static InternalTemperatureSensor instance;        // (1)
        return instance;
    }
};
