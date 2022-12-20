#pragma once
#include "led_strip.h"
#include <vector>
#include <array>

class LEDCtrl {
public:
  LEDCtrl();
  LEDCtrl(uint32_t gpio_pin, uint32_t number = 1);
  virtual ~LEDCtrl();

  void testled();
  void setTo(uint32_t idx, uint32_t r, uint32_t g, uint32_t b);
  std::array<uint32_t, 3> getLedColor(uint32_t idx) const;
  void clear();

  uint32_t getNumberOfLed() const;

private:
  led_strip_handle_t _led_handle = NULL;
  uint32_t _led_number{0};
  std::vector<std::array<uint32_t,3>> _led_colors{};
};
