#include "ledctrl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cassert>

LEDCtrl::LEDCtrl(){

}

LEDCtrl::LEDCtrl(uint32_t gpio_pin, uint32_t number) {
    assert(number > 0 && "number of leds must be greater than 0");
    _led_number = number;
    _led_colors.reserve(number);

    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_pin,
        .max_leds = number, // at least one LED on board
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(
        led_strip_new_rmt_device(&strip_config, &rmt_config, &_led_handle));
    ESP_ERROR_CHECK(led_strip_clear(_led_handle));
}

LEDCtrl::~LEDCtrl() { ESP_ERROR_CHECK(led_strip_del(_led_handle)); }

void LEDCtrl::testled() {
  TickType_t delay = 5'000 / portTICK_PERIOD_MS;
  setTo(0, 16, 0, 0);
  vTaskDelay(delay);
  setTo(0, 0, 0, 16);
  vTaskDelay(delay);
  setTo(0, 0, 16, 0);
  vTaskDelay(delay);
  clear();
}

void LEDCtrl::setTo(uint32_t idx, uint32_t r, uint32_t g, uint32_t b) {
    if (idx < _led_number) {
        ESP_ERROR_CHECK(led_strip_set_pixel(_led_handle, idx, r, g, b));
        ESP_ERROR_CHECK(led_strip_refresh(_led_handle));
        _led_colors[idx] = {r,g,b};
        ESP_LOGI("setTo", "led %lu to %lu, %lu, %lu", idx, r, g, b);
    } else {
        ESP_LOGE("LEDCtrl", "Led %lu not in range %lu", idx, _led_number);
    }
}

std::array<uint32_t, 3> LEDCtrl::getLedColor(uint32_t idx) const {
    if (idx < _led_number) {
        return _led_colors[idx];
    }
    else {
        ESP_LOGE("LEDCtrl", "Led %lu not in range %lu", idx, _led_number);
        return {};
    }
}

void LEDCtrl::clear() {
  ESP_ERROR_CHECK(led_strip_clear(_led_handle));
  //   ESP_LOGI("clear", "all leds cleared");
}

uint32_t LEDCtrl::getNumberOfLed() const {
    return _led_number;
}
