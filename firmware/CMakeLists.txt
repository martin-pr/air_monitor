cmake_minimum_required(VERSION 3.20)
project(air_monitor LANGUAGES NONE)

set(FQBN "esp32:esp32:XIAO_ESP32C3" CACHE STRING "Arduino FQBN")
set(PORT "/dev/ttyACM0" CACHE STRING "Upload port")

find_program(ARDUINO_CLI arduino-cli REQUIRED)

set(SKETCH_DIR   "${CMAKE_CURRENT_SOURCE_DIR}")
set(SKETCH_BUILD "${CMAKE_CURRENT_BINARY_DIR}/sketch")
set(SKETCH_BIN   "${SKETCH_BUILD}/firmware.ino.bin")

add_custom_command(
    OUTPUT ${SKETCH_BIN}
    COMMAND ${ARDUINO_CLI} compile
            --fqbn ${FQBN}
            --build-path ${SKETCH_BUILD}
            ${SKETCH_DIR}
    DEPENDS ${SKETCH_DIR}/firmware.ino
    COMMENT "arduino-cli compile (${FQBN})"
    VERBATIM
)

add_custom_target(build ALL DEPENDS ${SKETCH_BIN})

add_custom_target(flash
    COMMAND ${ARDUINO_CLI} upload
            --fqbn ${FQBN}
            --port ${PORT}
            --input-dir ${SKETCH_BUILD}
            ${SKETCH_DIR}
    DEPENDS build
    COMMENT "arduino-cli upload to ${PORT}"
    VERBATIM
)
