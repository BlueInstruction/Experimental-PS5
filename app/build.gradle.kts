import java.io.ByteArrayOutputStream

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.compose.compiler)
    alias(libs.plugins.ksp)
}

// Build identity, stamped into BuildConfig and emitted into the on-device
// diagnostic stream at every startup — so any pasted log self-identifies
// the exact APK, commit, and FEXCore pin it came from.
fun gitSha(): String = "unknown"

// Single source of truth: tools/fetch_fexcore.sh pins PIN_TAG + PIN_SHA;
// CMake stamps them into the native library and we stamp them here too.
fun fexCorePin(): String = "unknown"

android {
    namespace = "com.px5.emulator"
    compileSdk = 35
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "com.px5.emulator"
        minSdk = 28
        targetSdk = 35
        versionCode = 32
        versionName = "1.31"

        buildConfigField("String", "GIT_SHA", "\"${gitSha()}\"")
        buildConfigField("String", "FEXCORE_PIN", "\"${fexCorePin()}\"")
    }

    signingConfigs {
        create("debugConfig") {
            storeFile = file("${rootDir}/debug.keystore")
            storePassword = "android"
            keyAlias = "androiddebugkey"
            keyPassword = "android"
        }
    }
    buildTypes {
        debug {
            signingConfig = signingConfigs.getByName("debugConfig")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    // REQUIRED by libadrenotools: the hook lives in nativeLibraryDir which
    // only exists when .so files are decompressed into the APK install dir
    // (useLegacyPackaging = true). Same setting Winlator ships.
    packaging {
        jniLibs {
            useLegacyPackaging = true
        }
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3)
    implementation(libs.androidx.navigation.compose)

    implementation(libs.androidx.room.runtime)
    implementation(libs.androidx.room.ktx)
    ksp(libs.androidx.room.compiler)
}
dependencies {
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.7")
    // Full icon set (FolderOpen / Memory / BugReport / VolumeUp are absent
    // from material-icons-core).
    implementation("androidx.compose.material:material-icons-extended")
}

