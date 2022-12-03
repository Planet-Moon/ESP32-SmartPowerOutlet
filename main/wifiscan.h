#pragma once
#include "esp_wifi_types.h"
#include <string>
#include <vector>

extern "C" {
void print_auth_mode(int authmode);
void print_cipher_type(int pairwise_cipher, int group_cipher);
void wifi_scan(void);
}
void run_scan(void);

class WifiScanner {
public:
  std::vector<wifi_ap_record_t> getAps() { return _aps; }
  void scan();
  const wifi_ap_record_t *filterSSID(const std::string &ssid) const;
  static void logAp(const wifi_ap_record_t *record);

private:
  std::vector<wifi_ap_record_t> _aps;
};
