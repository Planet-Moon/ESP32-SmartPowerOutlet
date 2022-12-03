#pragma once
#include "esp_err.h"
#include "esp_http_server.h"

esp_err_t register_uris(httpd_handle_t& server);
