#include "InternalTemperatureSensor.h"
#include "esp_log.h"

InternalTemperatureSensor::InternalTemperatureSensor()
{
    temperature_sensor_config_t temp_sensor = {
        .range_min = 20,
        .range_max = 50,
    };
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor, &temp_handle));
}

InternalTemperatureSensor::~InternalTemperatureSensor()
{

}

float InternalTemperatureSensor::readTemperature() const {
    // Enable temperature sensor
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));
    // Get converted sensor data
    float tsens_out;
    ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_handle, &tsens_out));
    // Disable the temperature sensor if it's not needed and save the power
    ESP_ERROR_CHECK(temperature_sensor_disable(temp_handle));
    return tsens_out;
}
