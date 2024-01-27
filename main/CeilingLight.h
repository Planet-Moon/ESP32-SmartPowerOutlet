#pragma once
#include "IRSender.h"

class CeilingLight {
public:
    CeilingLight() = default;
    virtual ~CeilingLight() = default;

    esp_err_t initialize(gpio_num_t gpio_pin);

    void toggle_on();
    void toggle_night();
    void toggle_brt_m();
    void toggle_brt_p();
    void toggle_white();
    void toggle_warm();
    void toggle_mode();
    void toggle_percent100();

private:
    enum Command {
        ON = 0x4A,
        NIGHT = 0x2,
        BRT_M = 0x4F,
        BRT_P = 0x15,
        WHITE = 0x55,
        WARM = 0x4E,
        MODE = 0x1E,
        PERCENT_100 = 0x1F
    };
    IRSender irSender;
};
