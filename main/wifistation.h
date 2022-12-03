#pragma once

#include "esp_event.h"
// #include "esp_log.h"
// #include "esp_wifi.h"
// #include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
// #include "freertos/task.h"
#include <string>

class WifiStation {
public:
  WifiStation(const std::string &ssid, const std::string &password);
  virtual ~WifiStation();

  esp_err_t wifi_init_sta(void);

protected:
private:
  std::string _ssid;
  std::string _password;
};

esp_err_t connect_tcp_server(void);
