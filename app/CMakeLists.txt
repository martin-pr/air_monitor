cmake_minimum_required(VERSION 3.20)

project(AirMonitorWidget LANGUAGES NONE)

set(ANDROID_PACKAGE "com.example.airmonitor" CACHE STRING "Android application id")
set(ANDROID_MIN_SDK 23 CACHE STRING "Minimum Android SDK")
set(ANDROID_TARGET_SDK 34 CACHE STRING "Target Android SDK")

add_custom_target(apk ALL
    COMMAND "${CMAKE_COMMAND}"
        -DPROJECT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        -DPROJECT_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        -DANDROID_PACKAGE="${ANDROID_PACKAGE}"
        -DANDROID_MIN_SDK="${ANDROID_MIN_SDK}"
        -DANDROID_TARGET_SDK="${ANDROID_TARGET_SDK}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BuildAndroidApk.cmake"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/AirMonitorWidget-debug.apk"
    USES_TERMINAL
)
