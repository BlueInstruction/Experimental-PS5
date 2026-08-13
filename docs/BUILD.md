# PX5 Build Instructions

## Prerequisites
- Android Studio / Gradle
- NDK (Side by side) 25.1.8937393 or later
- CMake 3.22.1

## Architecture
PX5 uses a hybrid language architecture:
1. **Kotlin**: Used for Jetpack Compose UI, Android framework interactions, and high-level app logic.
2. **Java**: Used for JNI bindings and low-level interoperability with the native layer (e.g. `FexCoreWrapper.java`).
3. **C++ (NDK)**: Used for the core emulator components, including Vulkan rendering, GNM/GNMX translation, and CPU execution via FEXCore (`fexcore_wrapper.cpp`).

## Building
Run `./gradlew assembleDebug` to build the APK. The Gradle script is configured to automatically build the C++ layer using CMake.
