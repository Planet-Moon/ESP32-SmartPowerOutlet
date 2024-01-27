#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
// #include "esp_timer.h"

// NEC Format
class IRSender {
public:
    IRSender() = default;
    virtual ~IRSender() = default;

    esp_err_t initialize(gpio_num_t gpio_pin, uint8_t address, bool invert = false, bool MSB_first = true, uint8_t prescaler = 1);

    esp_err_t send_data(uint8_t data, uint8_t repeat = 0);
    esp_err_t carrier(uint32_t time, uint8_t prescaler = 1);

    void enable_log(bool value);
    const char* name() const;

private:

    bool _checkIsInitialized() const;

    gpio_num_t _gpio_pin = GPIO_NUM_0;
    char _name[22] = "";
    uint8_t _address = 0x0;
    bool _invert = false;
    bool _MSB_first = true;
    bool _initialized = false;
    bool _log_enabled = false;

    uint8_t _prescaler = 1;

    // esp_timer_handle_t _ir_sender_timer;

    // static void _timer_callback(void* arg);
    // uint64_t _timer_period = 13; // us
    // void _start_timer();
    // void _stop_timer();
    // void _add_timer_send_data(uint8_t address, uint8_t command);
    // bool _send_ready = false;
    // uint8_t _timer_sender_data[10] = {0};
};
