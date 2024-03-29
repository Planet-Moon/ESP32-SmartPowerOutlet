set(SRC
    "main.cpp"
    "ledctrl.cpp"
    "wifiscan.cpp"
    "wifistation.cpp"
    "https_handlers.cpp"
    "http_handlers.cpp"
    "server_uri.cpp"
    "logger.cpp"
    "fft.cpp"
    "MqttClient.cpp"
    "IRSender.cpp"
    "CeilingLight.cpp"
    "TimeMeasurement.cpp"
)

idf_component_register(
    SRCS ${SRC}

    INCLUDE_DIRS "."

    EMBED_FILES
        "serverfiles/color_picker.js.gz"
        "serverfiles/flowbite.js.gz"
        "serverfiles/flowbite.js.map.gz"
        "serverfiles/index.html.gz"
        "serverfiles/style.css.gz"
        "serverfiles/relais_switch.js.gz"
        "serverfiles/led_switch.js.gz"

    EMBED_TXTFILES
        "certs/servercert.pem"
        "certs/prvtkey.pem"

    REQUIRES
        nvs_flash
        driver
        led_strip
        esp_wifi
        esp_eth
        esp_https_server
        esp_http_server
        json
        esp_pm
        ntc
        internal_temperature_sensor
        mqtt
        esp_https_ota
        app_update
)
component_compile_definitions("-DUSE_NTC=1")
# add_compile_definitions(USE_NTC=1)
add_compile_definitions(_USE_MATH_DEFINES)
