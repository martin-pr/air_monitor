cmake_minimum_required(VERSION 3.20)
project(air_monitor LANGUAGES NONE)

set(FQBN "esp32:esp32:XIAO_ESP32C3" CACHE STRING "Arduino FQBN")
set(PORT "/dev/ttyACM0" CACHE STRING "Upload port")

find_program(ARDUINO_CLI arduino-cli REQUIRED)

# Shared Arduino library. Both variants pull it in via --library.
set(COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common")
set(COMMON_SOURCES
    ${COMMON_DIR}/library.properties
    ${COMMON_DIR}/src/battery.h       ${COMMON_DIR}/src/battery.cpp
    ${COMMON_DIR}/src/ble.h           ${COMMON_DIR}/src/ble.cpp
    ${COMMON_DIR}/src/reset_reason.h  ${COMMON_DIR}/src/reset_reason.cpp
    ${COMMON_DIR}/src/sensor.h        ${COMMON_DIR}/src/sensor.cpp
)

# ---- display variant ----
set(DISPLAY_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/display")
set(DISPLAY_BUILD "${CMAKE_CURRENT_BINARY_DIR}/display")
set(DISPLAY_BIN   "${DISPLAY_BUILD}/display.ino.bin")

add_custom_command(
    OUTPUT ${DISPLAY_BIN}
    COMMAND ${ARDUINO_CLI} compile
            --fqbn ${FQBN}
            --library ${COMMON_DIR}
            --build-path ${DISPLAY_BUILD}
            ${DISPLAY_DIR}
    DEPENDS ${DISPLAY_DIR}/display.ino
            ${DISPLAY_DIR}/app.h     ${DISPLAY_DIR}/app.cpp
            ${DISPLAY_DIR}/display.h ${DISPLAY_DIR}/display.cpp
            ${COMMON_SOURCES}
    COMMENT "arduino-cli compile display (${FQBN})"
    VERBATIM
)

add_custom_target(build_display DEPENDS ${DISPLAY_BIN})

add_custom_target(flash_display
    COMMAND ${ARDUINO_CLI} upload
            --fqbn ${FQBN}
            --port ${PORT}
            --input-dir ${DISPLAY_BUILD}
            ${DISPLAY_DIR}
    DEPENDS build_display
    COMMENT "arduino-cli upload display to ${PORT}"
    VERBATIM
)

# ---- led variant ----
set(LED_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/led")
set(LED_BUILD "${CMAKE_CURRENT_BINARY_DIR}/led")
set(LED_BIN   "${LED_BUILD}/led.ino.bin")

add_custom_command(
    OUTPUT ${LED_BIN}
    COMMAND ${ARDUINO_CLI} compile
            --fqbn ${FQBN}
            --library ${COMMON_DIR}
            --build-path ${LED_BUILD}
            ${LED_DIR}
    DEPENDS ${LED_DIR}/led.ino
            ${LED_DIR}/app.h ${LED_DIR}/app.cpp
            ${LED_DIR}/led.h ${LED_DIR}/led.cpp
            ${COMMON_SOURCES}
    COMMENT "arduino-cli compile led (${FQBN})"
    VERBATIM
)

add_custom_target(build_led DEPENDS ${LED_BIN})

add_custom_target(flash_led
    COMMAND ${ARDUINO_CLI} upload
            --fqbn ${FQBN}
            --port ${PORT}
            --input-dir ${LED_BUILD}
            ${LED_DIR}
    DEPENDS build_led
    COMMENT "arduino-cli upload led to ${PORT}"
    VERBATIM
)

# Default target: build both variants.
add_custom_target(build ALL DEPENDS ${DISPLAY_BIN} ${LED_BIN})
