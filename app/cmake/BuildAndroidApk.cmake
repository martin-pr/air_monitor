set(SDK_CANDIDATES "$ENV{ANDROID_HOME}" "$ENV{ANDROID_SDK_ROOT}" "$ENV{HOME}/Android/Sdk")

foreach(candidate IN LISTS SDK_CANDIDATES)
    if(candidate AND EXISTS "${candidate}/platforms/android-${ANDROID_TARGET_SDK}/android.jar")
        set(ANDROID_SDK "${candidate}")
        break()
    endif()
endforeach()

if(NOT ANDROID_SDK)
    message(FATAL_ERROR "Android SDK with platform android-${ANDROID_TARGET_SDK} not found. Set ANDROID_HOME or ANDROID_SDK_ROOT.")
endif()

file(GLOB build_tools_versions RELATIVE "${ANDROID_SDK}/build-tools" "${ANDROID_SDK}/build-tools/*")
list(SORT build_tools_versions COMPARE NATURAL ORDER DESCENDING)
list(GET build_tools_versions 0 build_tools_version)

set(BUILD_TOOLS "${ANDROID_SDK}/build-tools/${build_tools_version}")
set(AAPT2 "${BUILD_TOOLS}/aapt2")
set(D8 "${BUILD_TOOLS}/d8")
set(ZIPALIGN "${BUILD_TOOLS}/zipalign")
set(APKSIGNER "${BUILD_TOOLS}/apksigner")
set(ANDROID_JAR "${ANDROID_SDK}/platforms/android-${ANDROID_TARGET_SDK}/android.jar")

find_program(JAVAC javac REQUIRED)
find_program(JAR jar REQUIRED)
find_program(ZIP zip REQUIRED)
find_program(KEYTOOL keytool REQUIRED)

foreach(tool IN ITEMS AAPT2 D8 ZIPALIGN APKSIGNER)
    if(NOT EXISTS "${${tool}}")
        message(FATAL_ERROR "Missing Android build tool: ${${tool}}")
    endif()
endforeach()

set(BUILD_DIR "${PROJECT_BINARY_DIR}/android")
set(COMPILED_RES "${BUILD_DIR}/compiled-res.zip")
set(GEN_DIR "${BUILD_DIR}/gen")
set(CLASSES_DIR "${BUILD_DIR}/classes")
set(CLASSES_JAR "${BUILD_DIR}/classes.jar")
set(DEX_DIR "${BUILD_DIR}/dex")
set(UNSIGNED_APK "${BUILD_DIR}/unsigned.apk")
set(UNALIGNED_APK "${BUILD_DIR}/unaligned.apk")
set(ALIGNED_APK "${BUILD_DIR}/aligned.apk")
set(OUTPUT_APK "${PROJECT_BINARY_DIR}/AirMonitorWidget-debug.apk")
set(KEYSTORE "${PROJECT_BINARY_DIR}/debug.keystore")

file(REMOVE_RECURSE "${BUILD_DIR}")
file(MAKE_DIRECTORY "${BUILD_DIR}" "${GEN_DIR}" "${CLASSES_DIR}" "${DEX_DIR}")

execute_process(
    COMMAND "${AAPT2}" compile --dir "${PROJECT_SOURCE_DIR}/app/src/main/res" -o "${COMPILED_RES}"
    COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
    COMMAND "${AAPT2}" link
        -I "${ANDROID_JAR}"
        --manifest "${PROJECT_SOURCE_DIR}/app/src/main/AndroidManifest.xml"
        --java "${GEN_DIR}"
        --min-sdk-version "${ANDROID_MIN_SDK}"
        --target-sdk-version "${ANDROID_TARGET_SDK}"
        -o "${UNSIGNED_APK}"
        "${COMPILED_RES}"
    COMMAND_ERROR_IS_FATAL ANY
)

file(GLOB_RECURSE JAVA_SOURCES
    "${PROJECT_SOURCE_DIR}/app/src/main/java/*.java"
    "${GEN_DIR}/*.java"
)

execute_process(
    COMMAND "${JAVAC}"
        -source 8
        -target 8
        -bootclasspath "${ANDROID_JAR}"
        -d "${CLASSES_DIR}"
        ${JAVA_SOURCES}
    COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
    COMMAND "${JAR}" cf "${CLASSES_JAR}" -C "${CLASSES_DIR}" .
    COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
    COMMAND "${D8}" --lib "${ANDROID_JAR}" --output "${DEX_DIR}" "${CLASSES_JAR}"
    COMMAND_ERROR_IS_FATAL ANY
)

file(COPY_FILE "${UNSIGNED_APK}" "${UNALIGNED_APK}")
execute_process(
    COMMAND "${ZIP}" -j -q "${UNALIGNED_APK}" "${DEX_DIR}/classes.dex"
    COMMAND_ERROR_IS_FATAL ANY
)

execute_process(
    COMMAND "${ZIPALIGN}" -f 4 "${UNALIGNED_APK}" "${ALIGNED_APK}"
    COMMAND_ERROR_IS_FATAL ANY
)

if(NOT EXISTS "${KEYSTORE}")
    execute_process(
        COMMAND "${KEYTOOL}"
            -genkeypair
            -keystore "${KEYSTORE}"
            -storepass android
            -keypass android
            -alias androiddebugkey
            -keyalg RSA
            -keysize 2048
            -validity 10000
            -dname "CN=Android Debug,O=Android,C=US"
        COMMAND_ERROR_IS_FATAL ANY
    )
endif()

execute_process(
    COMMAND "${APKSIGNER}" sign
        --ks "${KEYSTORE}"
        --ks-pass pass:android
        --key-pass pass:android
        --out "${OUTPUT_APK}"
        "${ALIGNED_APK}"
    COMMAND_ERROR_IS_FATAL ANY
)

message(STATUS "Built ${OUTPUT_APK}")
