#include "CeilingLight.h"
#include "esp_log.h"


#define ADDRESS 0x20


esp_err_t CeilingLight::initialize(gpio_num_t gpio_pin) {
    return irSender.initialize(gpio_pin, ADDRESS, false, false, 1);
}

void CeilingLight::toggle_on() {
    ESP_LOGI("CeilingLight", "Toggle On");
    irSender.send_data(Command::ON);
}

void CeilingLight::toggle_night() {
    ESP_LOGI("CeilingLight", "Toggle night mode");
    irSender.send_data(Command::NIGHT);
}

void CeilingLight::toggle_brt_m() {
    ESP_LOGI("CeilingLight", "Toggle BRT-");
    irSender.send_data(Command::BRT_M);
}

void CeilingLight::toggle_brt_p() {
    ESP_LOGI("CeilingLight", "Toggle BRT+");
    irSender.send_data(Command::BRT_P);
}

void CeilingLight::toggle_white() {
    ESP_LOGI("CeilingLight", "Toggle White");
    irSender.send_data(Command::WHITE);
}

void CeilingLight::toggle_warm() {
    ESP_LOGI("CeilingLight", "Toggle Warm");
    irSender.send_data(Command::WARM);
}

void CeilingLight::toggle_mode() {
    ESP_LOGI("CeilingLight", "Toggle Mode");
    irSender.send_data(Command::MODE);
}

void CeilingLight::toggle_percent100() {
    ESP_LOGI("CeilingLight", "Toggle 100 Percent");
    irSender.send_data(Command::PERCENT_100);
}
