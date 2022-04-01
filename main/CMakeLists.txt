set(headers
    config.h
    debugconsole.h
    ota.h
    taskmanager.h
    webserver.h
    wifi.h
    espnow.h
)

set(sources
    config.cpp
    debugconsole.cpp
    main.cpp
    ota.cpp
    taskmanager.cpp
    webserver.cpp
    wifi.cpp
    espnow.cpp
)

set(dependencies
    freertos nvs_flash esp_http_server esp_https_ota mdns app_update esp_system esp_websocket_client driver
    arduino-esp32 ArduinoJson cpputils cxx-ring-buffer date espasynchttpreq espasyncota espchrono espcpputils
    espconfiglib esphttpdutils espwifistack expected fmt
)

idf_component_register(
    SRCS
        ${headers}
        ${sources}
    INCLUDE_DIRS
        .
    REQUIRES
        ${dependencies}
)

target_compile_options(${COMPONENT_TARGET}
    PRIVATE
        -fstack-reuse=all
        -fstack-protector-all
        -fdiagnostics-color=always
        -Wno-unused-function
        -Wno-deprecated-declarations
        -Wno-missing-field-initializers
        -Wno-parentheses
)
