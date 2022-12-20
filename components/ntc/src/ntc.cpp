#include "ntc.h"
#include "esp_log.h"
#include <esp_adc/adc_oneshot.h>
#include <cmath>

namespace ntc{
constexpr float zero_Celsius = 273.15; // Kelvin
constexpr float ntcTemperature(const float& R, const float& B, const float& T0, const float& R0){
    return (1/(1/(T0+zero_Celsius) + log(R/R0)/B))-zero_Celsius;
}

constexpr float vcc = 3.3;
constexpr float adc_factor = vcc/4095.f;
float readTemperature(){
    const int adc_value = readAdc();

    const float adc_V = adc_value * adc_factor;
    // ESP_LOGI("ADC1_CH3", "value: %.6f V", adc_V);

    const int series_resistor = 10'000;
    const float R = series_resistor/((4095/static_cast<float>(adc_value))-1);
    // ESP_LOGI("NTC R", "value: %.6f Ohm", R);

    const int nominal_resistor = 10'000;
    const int ntc_b = 3950;
    const float temperature = ntcTemperature(R, ntc_b, 25, nominal_resistor);
    // ESP_LOGI("NTC Temperature", "value: %.6f °C", temperature);
    return temperature;
}

int readAdc(){
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config;
    config.bitwidth = ADC_BITWIDTH_DEFAULT; // 12 Bit here - max 4095
    config.atten = ADC_ATTEN_DB_0;
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &config));

    int adc_value = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_3, &adc_value));
    // ESP_LOGI("ADC1_CH3", "raw value: %d", adc_value);

    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
    return adc_value;
}
}
