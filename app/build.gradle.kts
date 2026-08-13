plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.compose.compiler)
    alias(libs.plugins.ksp)
}

android {
    namespace = "com.px5.emulator"
    compileSdk = 35
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "com.px5.emulator"
        minSdk = 28
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"

        // FEXCore is a 64-bit-only codebase (it rejects 32-bit pointer sizes
        // with "Unsupported pointer size 4" at CMake configure time). PS5's CPU
        // is x86-64 and FEXCore translates x86-64 -> ARM64, so we only need the
        // arm64-v8a ABI. Building armeabi-v7a / x86 / x86_64 is pointless and
        // would fail in FEX's platform check.
        ndk {
            abiFilters += "arm64-v8a"
        }

        // Only build the px5 shared library target. By default AGP asks ninja
        // to build ALL CMake targets, including FEX's host tools (FEX, FEXBash,
        // FEXServer, FEXGetConfig, FEXOfflineCompiler, FEXRootFSFetcher,
        // FEXpidof) and FEXCore_shared. The host tools are Linux binaries that
        // fail to link against Bionic, and FEXCore_shared has an upstream bug
        // (only links FEXCore_Base under MINGW, missing fmt/xxhash/Allocator/
        // Config/LogMan deps on Linux/Android). We only need libpx5.so, which
        // statically links FEXCore + FEXCore_Base + JemallocDummy + deps.
        externalNativeBuild {
            cmake {
                targets += "px5"
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildFeatures {
        compose = true
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
}

