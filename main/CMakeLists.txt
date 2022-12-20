set(SRC
    "main.cpp"
    "ledctrl.cpp"
    "wifiscan.cpp"
    "wifistation.cpp"
    "https_handlers.cpp"
    "http_handlers.cpp"
    "server_uri.cpp"
    "logger.cpp"
)

idf_component_register(
    SRCS ${SRC}

    INCLUDE_DIRS "."

    EMBED_FILES "serverfiles/color_picker.js.gz"
        "serverfiles/flowbite.js.gz"
        "serverfiles/flowbite.js.map.gz"
        "serverfiles/index.html.gz"
        "serverfiles/style.css.gz"
        "serverfiles/relais_switch.js.gz"

    EMBED_TXTFILES "certs/servercert.pem"
                    "certs/prvtkey.pem"

    REQUIRES nvs_flash driver led_strip esp_wifi esp_eth esp_https_server esp_http_server json esp_pm ntc internal_temperature_sensor mqtt
)
