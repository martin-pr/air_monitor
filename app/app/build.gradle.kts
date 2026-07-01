import java.util.Properties

plugins {
    id("com.android.application")
}

// Optional upload-keystore config. If keystore.properties exists at the app
// root, release builds get signed with it; otherwise they fall back to the
// debug keystore (fine for local test builds, rejected by Play).
val keystorePropertiesFile = rootProject.file("keystore.properties")
val hasReleaseKeystore = keystorePropertiesFile.exists()
val keystoreProperties = Properties().apply {
    if (hasReleaseKeystore) {
        keystorePropertiesFile.inputStream().use { load(it) }
    }
}

android {
    namespace = "com.air_monitor"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.air_monitor"
        minSdk = 23
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
    }

    if (hasReleaseKeystore) {
        signingConfigs {
            create("release") {
                storeFile = rootProject.file(keystoreProperties["storeFile"] as String)
                storePassword = keystoreProperties["storePassword"] as String
                keyAlias = keystoreProperties["keyAlias"] as String
                keyPassword = keystoreProperties["keyPassword"] as String
            }
        }
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
        }
        release {
            isMinifyEnabled = false
            signingConfig = if (hasReleaseKeystore) {
                signingConfigs.getByName("release")
            } else {
                signingConfigs.getByName("debug")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
