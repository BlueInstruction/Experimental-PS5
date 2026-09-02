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
fun gitSha(): String = try {
    val out = ByteArrayOutputStream()
    exec {
        commandLine("git", "rev-parse", "--short", "HEAD")
        workingDir = rootProject.projectDir
        standardOutput = out
    }
    out.toString("UTF-8").trim().ifEmpty { "unknown" }
} catch (_: Throwable) {
    "unknown"   // tarball builds / CI edge cases: honest fallback
}

// Single source of truth: tools/fetch_fexcore.sh pins PIN_TAG + PIN_SHA;
// CMake stamps them into the native library and we stamp them here too.
fun fexCorePin(): String = try {
    val script = rootProject.projectDir.resolve("tools/fetch_fexcore.sh")
    val text = script.readText()
    val tag = Regex("PIN_TAG=\"([^\"]+)\"").find(text)?.groupValues?.get(1) ?: "unknown-tag"
    val sha = Regex("PIN_SHA=\"([^\"]+)\"").find(text)?.groupValues?.get(1)?.take(12) ?: "unknown-sha"
    "$tag @ $sha"
} catch (_: Throwable) {
    "unknown"
}

android {
    namespace = "com.px5.emulator"
    compileSdk = 35
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "com.px5.emulator"
        minSdk = 28
        targetSdk = 35
        versionCode = 37
        versionName = "1.36"

        buildConfigField("String", "GIT_SHA", "\"${gitSha()}\"")
        buildConfigField("String", "FEXCORE_PIN", "\"${fexCorePin()}\"")

        // Two-ABI strategy:
        //   arm64-v8a = the REAL guest engine (FEXCore JIT x86-64 -> ARM64).
        //   x86_64    = symbol-compatible UI-smoke library (stub/ui_smoke_stub.cpp)
        //               so the CI Emulator Smoke Test can install + launch this
        //               very APK on an x86_64 AVD (arm-only APKs fail with
        //               INSTALL_FAILED_NO_MATCHING_ABIS there).
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
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

                // Upstream FEX sources live OUTSIDE the repository; they are
                // materialized by tools/fetch_fexcore.sh at a pinned commit.
                val fexRoot = System.getenv("PX5_FEXCORE_ROOT")
                    ?: File(rootProject.projectDir, "../../deps/FEX").absolutePath
                arguments += "-DPX5_FEXCORE_ROOT=$fexRoot"

                // libadrenotools (Turnip loading) — same discipline as FEX.
                // Optional: when absent, driver switching honestly reports
                // system-ICD-only mode instead of faking success.
                val adrenoRoot = System.getenv("PX5_ADRENOTOOLS_ROOT")
                    ?: File(rootProject.projectDir, "../../deps/adrenotools").absolutePath
                arguments += "-DPX5_ADRENOTOOLS_ROOT=$adrenoRoot"
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

