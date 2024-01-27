#pragma once
#include "led_strip.h"
#include <vector>
#include <array>

class LEDCtrl {
public:
  LEDCtrl();
  void initialize(uint32_t gpio_pin, uint32_t number = 1);
  virtual ~LEDCtrl();

  struct LEDState{
    uint32_t r;
    uint32_t g;
    uint32_t b;
    bool on;
  };

  void testled();
  void setTo(uint32_t idx, uint32_t r, uint32_t g, uint32_t b);
  std::array<uint32_t, 3> getLedColor(uint32_t idx) const;
  LEDState getLedState(uint32_t idx) const;
  void clear();

  void turnOff(int32_t idx = -1);
  void turnOn(int32_t idx = -1);

  uint32_t getNumberOfLed() const;

private:
  led_strip_handle_t _led_handle = NULL;
  uint32_t _led_number{0};
  std::vector<LEDState> _led_colors{};
  bool _initialized = false;
  inline bool _isInitialized() const { return _initialized; }
  inline bool _checkInitialized() const;
};
