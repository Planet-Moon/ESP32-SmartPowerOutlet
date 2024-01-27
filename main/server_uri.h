#pragma once
#include "esp_err.h"
#include "esp_http_server.h"
#include "driver/gpio.h"
#include "ledctrl.h"

esp_err_t register_uris(httpd_handle_t& server, LEDCtrl* _myLed, const gpio_num_t* _RELAIS_PIN);
