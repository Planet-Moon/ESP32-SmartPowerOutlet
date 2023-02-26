#pragma once
#include "esp_http_server.h"

httpd_handle_t http_start_webserver();
void http_register_callbacks(httpd_handle_t& server);
