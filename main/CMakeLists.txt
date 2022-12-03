set(SRC
    "https_handlers.cpp"
    "http_handlers.cpp"
    "wifistation.cpp"
    "ledctrl.cpp"
    "main.cpp"
    "wifiscan.cpp"
    "ledctrl.cpp"
    "server_uri.cpp"
)

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "."
    EMBED_TXTFILES "certs/servercert.pem"
                    "certs/prvtkey.pem"
    REQUIRES nvs_flash driver led_strip esp_wifi esp_eth esp_https_server json
)
