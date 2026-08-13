# Build Summary

- gradle exit code: 1

## Matched failure patterns
```
325:Resource missing. [HTTP GET: https://dl.google.com/dl/android/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.pom]
327:Downloading https://repo.maven.apache.org/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.pom to /home/runner/.gradle/.tmp/gradle_download11875539785327803866bin
482:Downloading https://repo.maven.apache.org/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.jar to /home/runner/.gradle/.tmp/gradle_download16731300478278820024bin
641:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with InstrumentationAnalysisTransform
1050:Caching disabled for InstrumentationAnalysisTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
1360:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with InstrumentationAnalysisTransform
1361:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with MergeInstrumentationAnalysisTransform
3524:Caching disabled for MergeInstrumentationAnalysisTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
3527:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with ExternalDependencyInstrumentingArtifactTransform
3531:Caching disabled for ExternalDependencyInstrumentingArtifactTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
9071:C/C++: TypeError: argument of type 'NoneType' is not iterable
9192:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/utils/crash_handler.cpp:173:15: error: reinterpret_cast from '_Unwind_Word' (aka 'unsigned long') to 'unsigned long long' is not allowed
9198:> Task :app:buildCMakeDebug[arm64-v8a][px5] FAILED
9201:FAILURE: Build failed with an exception.
9352:  /home/runner/work/PSX5/PSX5/app/src/main/cpp/utils/crash_handler.cpp:173:15: error: reinterpret_cast from '_Unwind_Word' (aka 'unsigned long') to 'unsigned long long' is not allowed
9375:	at org.gradle.internal.Try$Failure.ifSuccessfulOrElse(Try.java:293)
9403:	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
9405:Caused by: org.gradle.internal.UncheckedException: com.android.ide.common.process.ProcessException: ninja: Entering directory `/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
9552:/home/runner/work/PSX5/PSX5/app/src/main/cpp/utils/crash_handler.cpp:173:15: error: reinterpret_cast from '_Unwind_Word' (aka 'unsigned long') to 'unsigned long long' is not allowed
9683:	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
9685:Caused by: com.android.ide.common.process.ProcessException: ninja: Entering directory `/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
9832:/home/runner/work/PSX5/PSX5/app/src/main/cpp/utils/crash_handler.cpp:173:15: error: reinterpret_cast from '_Unwind_Word' (aka 'unsigned long') to 'unsigned long long' is not allowed
9863:Caused by: com.android.ide.common.process.ProcessException: Error while executing process /usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja with arguments {-C /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a px5}
9868:Caused by: org.gradle.process.internal.ExecException: Process 'command '/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja'' finished with non-zero exit value 1
9874:BUILD FAILED in 4m 34s
```

## CMake / NDK specific errors
```
3697:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3698:C/C++: android.ndkPath from module build.gradle is not set
3699:C/C++: ndk.dir in local.properties is not set
3700:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3702:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3703:C/C++: android.ndkPath from module build.gradle is not set
3704:C/C++: ndk.dir in local.properties is not set
3705:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3707:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3708:C/C++: android.ndkPath from module build.gradle is not set
3709:C/C++: ndk.dir in local.properties is not set
3710:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3712:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3713:C/C++: android.ndkPath from module build.gradle is not set
3714:C/C++: ndk.dir in local.properties is not set
3715:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3717:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3718:C/C++: android.ndkPath from module build.gradle is not set
3719:C/C++: ndk.dir in local.properties is not set
3720:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3722:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3723:C/C++: android.ndkPath from module build.gradle is not set
3724:C/C++: ndk.dir in local.properties is not set
3725:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3727:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3728:C/C++: android.ndkPath from module build.gradle is not set
3729:C/C++: ndk.dir in local.properties is not set
3730:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3732:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3733:C/C++: android.ndkPath from module build.gradle is not set
3734:C/C++: ndk.dir in local.properties is not set
3735:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3737:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3738:C/C++: android.ndkPath from module build.gradle is not set
3739:C/C++: ndk.dir in local.properties is not set
3740:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3742:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3743:C/C++: android.ndkPath from module build.gradle is not set
3744:C/C++: ndk.dir in local.properties is not set
3745:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3747:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3748:C/C++: android.ndkPath from module build.gradle is not set
3749:C/C++: ndk.dir in local.properties is not set
3750:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3752:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3753:C/C++: android.ndkPath from module build.gradle is not set
3754:C/C++: ndk.dir in local.properties is not set
3755:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3757:C/C++: android.ndkVersion from module build.gradle is [27.3.13750724]
3758:C/C++: android.ndkPath from module build.gradle is not set
3759:C/C++: ndk.dir in local.properties is not set
3760:C/C++: Not considering ANDROID_NDK_HOME because support was removed after deprecation period.
3770:Tasks to be executed: [task ':app:preBuild', task ':app:preDebugBuild', task ':app:mergeDebugNativeDebugMetadata', task ':app:checkKotlinGradlePluginConfigurationErrors', task ':app:checkDebugAarMetadata', task ':app:generateDebugResValues', task ':app:mapDebugSourceSetPaths', task ':app:generateDebugResources', task ':app:mergeDebugResources', task ':app:packageDebugResources', task ':app:parseDebugLocalResources', task ':app:createDebugCompatibleScreenManifests', task ':app:extractDeepLinksDebug', task ':app:processDebugMainManifest', task ':app:processDebugManifest', task ':app:processDebugManifestForPackage', task ':app:processDebugResources', task ':app:kspDebugKotlin', task ':app:compileDebugKotlin', task ':app:javaPreCompileDebug', task ':app:compileDebugJavaWithJavac', task ':app:mergeDebugShaders', task ':app:compileDebugShaders', task ':app:generateDebugAssets', task ':app:mergeDebugAssets', task ':app:compressDebugAssets', task ':app:desugarDebugFileDependencies', task ':app:dexBuilderDebug', task ':app:mergeDebugGlobalSynthetics', task ':app:processDebugJavaRes', task ':app:mergeDebugJavaResource', task ':app:checkDebugDuplicateClasses', task ':app:mergeExtDexDebug', task ':app:mergeLibDexDebug', task ':app:mergeProjectDexDebug', task ':app:configureCMakeDebug[arm64-v8a]', task ':app:buildCMakeDebug[arm64-v8a][px5]', task ':app:mergeDebugJniLibFolders', task ':app:mergeDebugNativeLibs', task ':app:stripDebugDebugSymbols', task ':app:validateSigningDebug', task ':app:writeDebugAppMetadata', task ':app:writeDebugSigningConfigVersions', task ':app:packageDebug', task ':app:createDebugApkListingFileRedirect', task ':app:assembleDebug']
8814:INFO: D8: Some warnings are typically a sign of using an outdated Java toolchain. To fix, recompile the source with an updated toolchain.
8857:Resolve mutations for :app:configureCMakeDebug[arm64-v8a] (Thread[Execution worker,5,main]) started.
8858::app:configureCMakeDebug[arm64-v8a] (Thread[Execution worker,5,main]) started.
8975:> Task :app:configureCMakeDebug[arm64-v8a]
8976:Caching disabled for task ':app:configureCMakeDebug[arm64-v8a]' because:
8979:Task ':app:configureCMakeDebug[arm64-v8a]' is not up-to-date because:
8987:C/C++: Parsing /usr/local/lib/android/sdk/cmake/3.22.1/package.xml
8988:C/C++: Parsing /usr/local/lib/android/sdk/cmake/3.31.5/package.xml
8989:C/C++: Parsing /usr/local/lib/android/sdk/cmake/4.1.2/package.xml
8995:C/C++: Parsing /usr/local/lib/android/sdk/ndk/27.3.13750724/package.xml
8996:C/C++: Parsing /usr/local/lib/android/sdk/ndk/28.2.13676358/package.xml
8997:C/C++: Parsing /usr/local/lib/android/sdk/ndk/29.0.14206865/package.xml
9013:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : Start JSON generation. Platform version: 28 min SDK version: arm64-v8a
9014:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : rebuilding JSON /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/android_gradle_build.json due to:
9015:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : - no fingerprint file, will remove stale configuration folder
9016:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : removing stale contents from '/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
9017:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : created folder '/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
9018:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : executing cmake /usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake \
9020:  -DCMAKE_SYSTEM_NAME=Android \
9021:  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
9022:  -DCMAKE_SYSTEM_VERSION=28 \
9025:  -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
9026:  -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
9027:  -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
9028:  -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake \
9029:  -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja \
9030:  -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a \
(no matches)
```

## Last 400 lines
```
[70/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/softfloat_raiseFlags.c.o
[71/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_commonNaNToExtF80UI.c.o
[72/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalF64Sig.c.o
[73/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f64_to_extF80.c.o
[74/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalF32Sig.c.o
[75/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_f64UIToCommonNaN.c.o
[76/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_eq.c.o
[77/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_roundToInt.c.o
[78/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f32_to_extF80.c.o
[79/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_lt.c.o
[80/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_f32UIToCommonNaN.c.o
[81/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/exp2ll.c.o
[82/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/log2ll.c.o
[83/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/constll.c.o
[84/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/floorll.c.o
[85/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/polevll.c.o
[86/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/atanll.c.o
[87/189] Linking C static library fex/External/SoftFloat-3e/libsoftfloat_3e.a
[88/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/mtherr.c.o
[89/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/sinll.c.o
[90/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/tanll.c.o
[91/189] Building CXX object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/Impl.cpp.o
[92/189] Linking CXX static library fex/External/cephes/libcephes_128bit.a
[93/189] Generating ../../../IR.md
[94/189] Building CXX object fex/FEXCore/Source/CMakeFiles/JemallocDummy.dir/Utils/AllocatorHooks.cpp.o
[95/189] Linking CXX static library fex/FEXCore/Source/libJemallocDummy.a
[96/189] Generating ../../../include/FEXCore/IR/IRDefines.inc, ../../../include/FEXCore/IR/IRDefines_Dispatch.inc
[97/189] Generating ../../../include/FEXCore/Config/ConfigValues.inl, ../../../include/FEXCore/Config/ConfigOptions.inl, ../../../generated/FEX.1
[98/189] Generating ../../../generated/FEX.1.gz
[99/189] Building CXX object fex/External/fmt/CMakeFiles/fmt.dir/src/format.cc.o
[100/189] Building CXX object CMakeFiles/px5.dir/core/emulator.cpp.o
[101/189] Building CXX object CMakeFiles/px5.dir/fexcore_wrapper.cpp.o
[102/189] Building CXX object CMakeFiles/px5.dir/fexcore_integration.cpp.o
[103/189] Building CXX object CMakeFiles/px5.dir/kernel/signals.cpp.o
[104/189] Building CXX object CMakeFiles/px5.dir/memory/memory.cpp.o
[105/189] Building CXX object CMakeFiles/px5.dir/memory/memory_map.cpp.o
[106/189] Building CXX object CMakeFiles/px5.dir/kernel/syscalls.cpp.o
[107/189] Building CXX object CMakeFiles/px5.dir/kernel_hle.cpp.o
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:63:100: warning: format specifies type 'unsigned long long' but the argument has type 'uint64_t' (aka 'unsigned long') [-Wformat]
   63 |             LOGI("Kernel HLE: Intercepted Syscall #%u (args: 0x%llx, 0x%llx, 0x%llx)", syscallNum, arg1, arg2, arg3);
      |                                                                ~~~~                                ^~~~
      |                                                                %lx
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:6:66: note: expanded from macro 'LOGI'
    6 | #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
      |                                                                  ^~~~~~~~~~~
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:63:106: warning: format specifies type 'unsigned long long' but the argument has type 'uint64_t' (aka 'unsigned long') [-Wformat]
   63 |             LOGI("Kernel HLE: Intercepted Syscall #%u (args: 0x%llx, 0x%llx, 0x%llx)", syscallNum, arg1, arg2, arg3);
      |                                                                        ~~~~                              ^~~~
      |                                                                        %lx
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:6:66: note: expanded from macro 'LOGI'
    6 | #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
      |                                                                  ^~~~~~~~~~~
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:63:112: warning: format specifies type 'unsigned long long' but the argument has type 'uint64_t' (aka 'unsigned long') [-Wformat]
   63 |             LOGI("Kernel HLE: Intercepted Syscall #%u (args: 0x%llx, 0x%llx, 0x%llx)", syscallNum, arg1, arg2, arg3);
      |                                                                                ~~~~                            ^~~~
      |                                                                                %lx
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:6:66: note: expanded from macro 'LOGI'
    6 | #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
      |                                                                  ^~~~~~~~~~~
3 warnings generated.
[108/189] Building CXX object CMakeFiles/px5.dir/audio/audio.cpp.o
[109/189] Building CXX object CMakeFiles/px5.dir/gpu/vulkan_device.cpp.o
[110/189] Building CXX object CMakeFiles/px5.dir/loader/elf_loader.cpp.o
[111/189] Building CXX object CMakeFiles/px5.dir/gpu/shader_cache.cpp.o
[112/189] Building CXX object CMakeFiles/px5.dir/loader/self_loader.cpp.o
[113/189] Building CXX object CMakeFiles/px5.dir/gnm_vulkan_renderer.cpp.o
[114/189] Building CXX object CMakeFiles/px5.dir/filesystem/vfs.cpp.o
[115/189] Building CXX object CMakeFiles/px5.dir/audio_input_native.cpp.o
[116/189] Building CXX object CMakeFiles/px5.dir/input/controller.cpp.o
[117/189] Building CXX object CMakeFiles/px5.dir/turnip_hook.cpp.o
[118/189] Building CXX object CMakeFiles/px5.dir/utils/logger.cpp.o
[119/189] Building CXX object CMakeFiles/px5.dir/utils/linker_ns_bypass.cpp.o
[120/189] Linking CXX static library fex/External/fmt/libfmtd.a
[121/189] Building CXX object CMakeFiles/px5.dir/utils/crash_handler.cpp.o
FAILED: CMakeFiles/px5.dir/utils/crash_handler.cpp.o 
/usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ --target=aarch64-none-linux-android28 --sysroot=/usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/sysroot -DFEXCORE_HAS_PRESERVE_ALL_ATTR=1 -DFEXCORE_PRESERVE_ALL_ATTR="__attribute__((preserve_all))" -DHAS_SYSCALL_GETTID=1 -DHAS_SYSCALL_TGKILL=1 -DINLINE="static inline" -DINLINE_LEVEL=4 -DSOFTFLOAT_BUILTIN_CLZ=1 -DSOFTFLOAT_FAST_DIV32TO16=1 -DSOFTFLOAT_FAST_DIV64TO32=1 -DSOFTFLOAT_FAST_INT64=1 -Dpx5_EXPORTS -I/home/runner/work/PSX5/PSX5/app/src/main/cpp/../../../../third_party/fex/FEXCore/include -I/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/fex/FEXCore/include -I/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/fex/generated -I/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/fex/FEXCore/Source -I/home/runner/work/PSX5/PSX5/third_party/fex/FEXCore/include -I/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/unordered_dense/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/fmt/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/xxhash/cmake_unofficial/.. -I/home/runner/work/PSX5/PSX5/third_party/fex/FEXHeaderUtils/. -I/home/runner/work/PSX5/PSX5/third_party/fex/CodeEmitter/. -I/home/runner/work/PSX5/PSX5/third_party/fex/External/cephes/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/SoftFloat-3e/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/SoftFloat-3e/include/SoftFloat-3e -g -DANDROID -fdata-sections -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -Wformat -Werror=format-security   -fno-limit-debug-info  -fPIC -include /home/runner/work/PSX5/PSX5/app/src/main/cpp/compat/std_atomic_ref_polyfill.h -std=c++20 -MD -MT CMakeFiles/px5.dir/utils/crash_handler.cpp.o -MF CMakeFiles/px5.dir/utils/crash_handler.cpp.o.d -o CMakeFiles/px5.dir/utils/crash_handler.cpp.o -c /home/runner/work/PSX5/PSX5/app/src/main/cpp/utils/crash_handler.cpp
/home/runner/work/PSX5/PSX5/app/src/main/cpp/utils/crash_handler.cpp:173:15: error: reinterpret_cast from '_Unwind_Word' (aka 'unsigned long') to 'unsigned long long' is not allowed
  173 |     auto pc = reinterpret_cast<unsigned long long>(_Unwind_GetIP(ctx));
      |               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1 error generated.
[122/189] Building CXX object CMakeFiles/px5.dir/media/media_codec_hle.cpp.o
[123/189] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Utils/FileLoading.cpp.o
ninja: build stopped: subcommand failed.

C++ build system [build] failed while executing:
    /usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja \
      -C \
      /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a \
      px5
  from /home/runner/work/PSX5/PSX5/app
	at org.gradle.internal.UncheckedException.throwAsUncheckedException(UncheckedException.java:69)
	at org.gradle.internal.UncheckedException.throwAsUncheckedException(UncheckedException.java:42)
	at org.gradle.internal.reflect.JavaMethod.invoke(JavaMethod.java:128)
	at org.gradle.api.internal.project.taskfactory.StandardTaskAction.doExecute(StandardTaskAction.java:58)
	at org.gradle.api.internal.project.taskfactory.StandardTaskAction.execute(StandardTaskAction.java:51)
	at org.gradle.api.internal.project.taskfactory.StandardTaskAction.execute(StandardTaskAction.java:29)
	at org.gradle.api.internal.tasks.execution.TaskExecution$3.run(TaskExecution.java:244)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$1.execute(DefaultBuildOperationRunner.java:29)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$1.execute(DefaultBuildOperationRunner.java:26)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:66)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:166)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.run(DefaultBuildOperationRunner.java:47)
	at org.gradle.api.internal.tasks.execution.TaskExecution.executeAction(TaskExecution.java:229)
	at org.gradle.api.internal.tasks.execution.TaskExecution.executeActions(TaskExecution.java:212)
	at org.gradle.api.internal.tasks.execution.TaskExecution.executeWithPreviousOutputFiles(TaskExecution.java:195)
	at org.gradle.api.internal.tasks.execution.TaskExecution.execute(TaskExecution.java:162)
	at org.gradle.internal.execution.steps.ExecuteStep.executeInternal(ExecuteStep.java:105)
	at org.gradle.internal.execution.steps.ExecuteStep.access$000(ExecuteStep.java:44)
	at org.gradle.internal.execution.steps.ExecuteStep$1.call(ExecuteStep.java:59)
	at org.gradle.internal.execution.steps.ExecuteStep$1.call(ExecuteStep.java:56)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:209)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:204)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:66)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:166)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.call(DefaultBuildOperationRunner.java:53)
	at org.gradle.internal.execution.steps.ExecuteStep.execute(ExecuteStep.java:56)
	at org.gradle.internal.execution.steps.ExecuteStep.execute(ExecuteStep.java:44)
	at org.gradle.internal.execution.steps.CancelExecutionStep.execute(CancelExecutionStep.java:42)
	at org.gradle.internal.execution.steps.TimeoutStep.executeWithoutTimeout(TimeoutStep.java:75)
	at org.gradle.internal.execution.steps.TimeoutStep.execute(TimeoutStep.java:55)
	at org.gradle.internal.execution.steps.PreCreateOutputParentsStep.execute(PreCreateOutputParentsStep.java:50)
	at org.gradle.internal.execution.steps.PreCreateOutputParentsStep.execute(PreCreateOutputParentsStep.java:28)
	at org.gradle.internal.execution.steps.RemovePreviousOutputsStep.execute(RemovePreviousOutputsStep.java:67)
	at org.gradle.internal.execution.steps.RemovePreviousOutputsStep.execute(RemovePreviousOutputsStep.java:37)
	at org.gradle.internal.execution.steps.BroadcastChangingOutputsStep.execute(BroadcastChangingOutputsStep.java:61)
	at org.gradle.internal.execution.steps.BroadcastChangingOutputsStep.execute(BroadcastChangingOutputsStep.java:26)
	at org.gradle.internal.execution.steps.CaptureOutputsAfterExecutionStep.execute(CaptureOutputsAfterExecutionStep.java:69)
	at org.gradle.internal.execution.steps.CaptureOutputsAfterExecutionStep.execute(CaptureOutputsAfterExecutionStep.java:46)
	at org.gradle.internal.execution.steps.ResolveInputChangesStep.execute(ResolveInputChangesStep.java:40)
	at org.gradle.internal.execution.steps.ResolveInputChangesStep.execute(ResolveInputChangesStep.java:29)
	at org.gradle.internal.execution.steps.BuildCacheStep.executeWithoutCache(BuildCacheStep.java:189)
	at org.gradle.internal.execution.steps.BuildCacheStep.lambda$execute$1(BuildCacheStep.java:75)
	at org.gradle.internal.Either$Right.fold(Either.java:175)
	at org.gradle.internal.execution.caching.CachingState.fold(CachingState.java:62)
	at org.gradle.internal.execution.steps.BuildCacheStep.execute(BuildCacheStep.java:73)
	at org.gradle.internal.execution.steps.BuildCacheStep.execute(BuildCacheStep.java:48)
	at org.gradle.internal.execution.steps.StoreExecutionStateStep.execute(StoreExecutionStateStep.java:46)
	at org.gradle.internal.execution.steps.StoreExecutionStateStep.execute(StoreExecutionStateStep.java:35)
	at org.gradle.internal.execution.steps.SkipUpToDateStep.executeBecause(SkipUpToDateStep.java:75)
	at org.gradle.internal.execution.steps.SkipUpToDateStep.lambda$execute$2(SkipUpToDateStep.java:53)
	at org.gradle.internal.execution.steps.SkipUpToDateStep.execute(SkipUpToDateStep.java:53)
	at org.gradle.internal.execution.steps.SkipUpToDateStep.execute(SkipUpToDateStep.java:35)
	at org.gradle.internal.execution.steps.legacy.MarkSnapshottingInputsFinishedStep.execute(MarkSnapshottingInputsFinishedStep.java:37)
	at org.gradle.internal.execution.steps.legacy.MarkSnapshottingInputsFinishedStep.execute(MarkSnapshottingInputsFinishedStep.java:27)
	at org.gradle.internal.execution.steps.ResolveIncrementalCachingStateStep.executeDelegate(ResolveIncrementalCachingStateStep.java:49)
	at org.gradle.internal.execution.steps.ResolveIncrementalCachingStateStep.executeDelegate(ResolveIncrementalCachingStateStep.java:27)
	at org.gradle.internal.execution.steps.AbstractResolveCachingStateStep.execute(AbstractResolveCachingStateStep.java:71)
	at org.gradle.internal.execution.steps.AbstractResolveCachingStateStep.execute(AbstractResolveCachingStateStep.java:39)
	at org.gradle.internal.execution.steps.ResolveChangesStep.execute(ResolveChangesStep.java:65)
	at org.gradle.internal.execution.steps.ResolveChangesStep.execute(ResolveChangesStep.java:36)
	at org.gradle.internal.execution.steps.ValidateStep.execute(ValidateStep.java:105)
	at org.gradle.internal.execution.steps.ValidateStep.execute(ValidateStep.java:54)
	at org.gradle.internal.execution.steps.AbstractCaptureStateBeforeExecutionStep.execute(AbstractCaptureStateBeforeExecutionStep.java:64)
	at org.gradle.internal.execution.steps.AbstractCaptureStateBeforeExecutionStep.execute(AbstractCaptureStateBeforeExecutionStep.java:43)
	at org.gradle.internal.execution.steps.AbstractSkipEmptyWorkStep.executeWithNonEmptySources(AbstractSkipEmptyWorkStep.java:125)
	at org.gradle.internal.execution.steps.AbstractSkipEmptyWorkStep.execute(AbstractSkipEmptyWorkStep.java:56)
	at org.gradle.internal.execution.steps.AbstractSkipEmptyWorkStep.execute(AbstractSkipEmptyWorkStep.java:36)
	at org.gradle.internal.execution.steps.legacy.MarkSnapshottingInputsStartedStep.execute(MarkSnapshottingInputsStartedStep.java:38)
	at org.gradle.internal.execution.steps.LoadPreviousExecutionStateStep.execute(LoadPreviousExecutionStateStep.java:36)
	at org.gradle.internal.execution.steps.LoadPreviousExecutionStateStep.execute(LoadPreviousExecutionStateStep.java:23)
	at org.gradle.internal.execution.steps.HandleStaleOutputsStep.execute(HandleStaleOutputsStep.java:75)
	at org.gradle.internal.execution.steps.HandleStaleOutputsStep.execute(HandleStaleOutputsStep.java:41)
	at org.gradle.internal.execution.steps.AssignMutableWorkspaceStep.lambda$execute$0(AssignMutableWorkspaceStep.java:35)
	at org.gradle.api.internal.tasks.execution.TaskExecution$4.withWorkspace(TaskExecution.java:289)
	at org.gradle.internal.execution.steps.AssignMutableWorkspaceStep.execute(AssignMutableWorkspaceStep.java:31)
	at org.gradle.internal.execution.steps.AssignMutableWorkspaceStep.execute(AssignMutableWorkspaceStep.java:22)
	at org.gradle.internal.execution.steps.ChoosePipelineStep.execute(ChoosePipelineStep.java:40)
	at org.gradle.internal.execution.steps.ChoosePipelineStep.execute(ChoosePipelineStep.java:23)
	at org.gradle.internal.execution.steps.ExecuteWorkBuildOperationFiringStep.lambda$execute$2(ExecuteWorkBuildOperationFiringStep.java:67)
	at org.gradle.internal.execution.steps.ExecuteWorkBuildOperationFiringStep.execute(ExecuteWorkBuildOperationFiringStep.java:67)
	at org.gradle.internal.execution.steps.ExecuteWorkBuildOperationFiringStep.execute(ExecuteWorkBuildOperationFiringStep.java:39)
	at org.gradle.internal.execution.steps.IdentityCacheStep.execute(IdentityCacheStep.java:46)
	at org.gradle.internal.execution.steps.IdentityCacheStep.execute(IdentityCacheStep.java:34)
	at org.gradle.internal.execution.steps.IdentifyStep.execute(IdentifyStep.java:48)
	at org.gradle.internal.execution.steps.IdentifyStep.execute(IdentifyStep.java:35)
	at org.gradle.internal.execution.impl.DefaultExecutionEngine$1.execute(DefaultExecutionEngine.java:61)
	at org.gradle.api.internal.tasks.execution.ExecuteActionsTaskExecuter.executeIfValid(ExecuteActionsTaskExecuter.java:127)
	at org.gradle.api.internal.tasks.execution.ExecuteActionsTaskExecuter.execute(ExecuteActionsTaskExecuter.java:116)
	at org.gradle.api.internal.tasks.execution.FinalizePropertiesTaskExecuter.execute(FinalizePropertiesTaskExecuter.java:46)
	at org.gradle.api.internal.tasks.execution.ResolveTaskExecutionModeExecuter.execute(ResolveTaskExecutionModeExecuter.java:51)
	at org.gradle.api.internal.tasks.execution.SkipTaskWithNoActionsExecuter.execute(SkipTaskWithNoActionsExecuter.java:57)
	at org.gradle.api.internal.tasks.execution.SkipOnlyIfTaskExecuter.execute(SkipOnlyIfTaskExecuter.java:74)
	at org.gradle.api.internal.tasks.execution.CatchExceptionTaskExecuter.execute(CatchExceptionTaskExecuter.java:36)
	at org.gradle.api.internal.tasks.execution.EventFiringTaskExecuter$1.executeTask(EventFiringTaskExecuter.java:77)
	at org.gradle.api.internal.tasks.execution.EventFiringTaskExecuter$1.call(EventFiringTaskExecuter.java:55)
	at org.gradle.api.internal.tasks.execution.EventFiringTaskExecuter$1.call(EventFiringTaskExecuter.java:52)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:209)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:204)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:66)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:166)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.call(DefaultBuildOperationRunner.java:53)
	at org.gradle.api.internal.tasks.execution.EventFiringTaskExecuter.execute(EventFiringTaskExecuter.java:52)
	at org.gradle.execution.plan.LocalTaskNodeExecutor.execute(LocalTaskNodeExecutor.java:42)
	at org.gradle.execution.taskgraph.DefaultTaskExecutionGraph$InvokeNodeExecutorsAction.execute(DefaultTaskExecutionGraph.java:331)
	at org.gradle.execution.taskgraph.DefaultTaskExecutionGraph$InvokeNodeExecutorsAction.execute(DefaultTaskExecutionGraph.java:318)
	at org.gradle.execution.taskgraph.DefaultTaskExecutionGraph$BuildOperationAwareExecutionAction.lambda$execute$0(DefaultTaskExecutionGraph.java:314)
	at org.gradle.internal.operations.CurrentBuildOperationRef.with(CurrentBuildOperationRef.java:85)
	at org.gradle.execution.taskgraph.DefaultTaskExecutionGraph$BuildOperationAwareExecutionAction.execute(DefaultTaskExecutionGraph.java:314)
	at org.gradle.execution.taskgraph.DefaultTaskExecutionGraph$BuildOperationAwareExecutionAction.execute(DefaultTaskExecutionGraph.java:303)
	at org.gradle.execution.plan.DefaultPlanExecutor$ExecutorWorker.execute(DefaultPlanExecutor.java:459)
	at org.gradle.execution.plan.DefaultPlanExecutor$ExecutorWorker.run(DefaultPlanExecutor.java:376)
	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
	at org.gradle.internal.concurrent.AbstractManagedExecutor$1.run(AbstractManagedExecutor.java:48)
Caused by: com.android.ide.common.process.ProcessException: ninja: Entering directory `/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
[0/2] Re-checking globbed directories...
[1/189] Building C object fex/External/xxhash/cmake_unofficial/CMakeFiles/xxhash.dir/home/runner/work/PSX5/PSX5/third_party/fex/External/xxhash/xxhash.c.o
[2/189] Linking C static library fex/External/xxhash/cmake_unofficial/libxxhash.a
[3/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_eq_signaling.c.o
[4/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_add.c.o
[5/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_div.c.o
[6/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_sub.c.o
[7/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_mul.c.o
[8/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_rem.c.o
[9/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_sqrt.c.o
[10/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_le.c.o
[11/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_i32.c.o
[12/189] Building CXX object fex/External/fmt/CMakeFiles/fmt.dir/src/os.cc.o
[13/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_i64.c.o
[14/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_ui64.c.o
[15/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_f64.c.o
[16/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_f32.c.o
[17/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/i32_to_extF80.c.o
[18/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/ui64_to_extF80.c.o
[19/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_f128.c.o
[20/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_extF80.c.o
[21/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_add.c.o
[22/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_div.c.o
[23/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_isSignalingNaN.c.o
[24/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_eq.c.o
[25/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_lt_quiet.c.o
[26/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_le.c.o
[27/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_le_quiet.c.o
[28/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normRoundPackToF128.c.o
[29/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_lt.c.o
[30/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_mulAdd.c.o
[31/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_mul.c.o
[32/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_rem.c.o
[33/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_sqrt.c.o
[34/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_sub.c.o
[35/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_f16.c.o
[36/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_f32.c.o
[37/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_i32.c.o
[38/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_f64.c.o
[39/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_i64.c.o
[40/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_ui64.c.o
[41/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_ui32.c.o
[42/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_addMagsF128.c.o
[43/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_propagateNaNF128UI.c.o
[44/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_subMagsF128.c.o
[45/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundPackToF128.c.o
[46/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_f128UIToCommonNaN.c.o
[47/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f32_to_f128.c.o
[48/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundToUI64.c.o
[49/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/i32_to_f128.c.o
[50/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_commonNaNToF128UI.c.o
[51/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_extF80UIToCommonNaN.c.o
[52/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalF128Sig.c.o
[53/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundToI32.c.o
[54/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundPackToF32.c.o
[55/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundToI64.c.o
[56/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_addMagsExtF80.c.o
[57/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_commonNaNToF64UI.c.o
[58/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_commonNaNToF32UI.c.o
[59/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_propagateNaNExtF80UI.c.o
[60/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundPackToF64.c.o
[61/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalExtF80Sig.c.o
[62/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundPackToExtF80.c.o
[63/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_shiftRightJam128.c.o
[64/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_subMagsExtF80.c.o
[65/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_shiftRightJam128Extra.c.o
[66/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normRoundPackToExtF80.c.o
[67/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_approxRecipSqrt32_1.c.o
[68/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_approxRecip_1Ks.c.o
[69/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_approxRecipSqrt_1Ks.c.o
[70/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/softfloat_raiseFlags.c.o
[71/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_commonNaNToExtF80UI.c.o
[72/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalF64Sig.c.o
[73/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f64_to_extF80.c.o
[74/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalF32Sig.c.o
[75/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_f64UIToCommonNaN.c.o
[76/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_eq.c.o
[77/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_roundToInt.c.o
[78/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f32_to_extF80.c.o
[79/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_lt.c.o
[80/189] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_f32UIToCommonNaN.c.o
[81/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/exp2ll.c.o
[82/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/log2ll.c.o
[83/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/constll.c.o
[84/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/floorll.c.o
[85/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/polevll.c.o
[86/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/atanll.c.o
[87/189] Linking C static library fex/External/SoftFloat-3e/libsoftfloat_3e.a
[88/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/mtherr.c.o
[89/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/sinll.c.o
[90/189] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/tanll.c.o
[91/189] Building CXX object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/Impl.cpp.o
[92/189] Linking CXX static library fex/External/cephes/libcephes_128bit.a
[93/189] Generating ../../../IR.md
[94/189] Building CXX object fex/FEXCore/Source/CMakeFiles/JemallocDummy.dir/Utils/AllocatorHooks.cpp.o
[95/189] Linking CXX static library fex/FEXCore/Source/libJemallocDummy.a
[96/189] Generating ../../../include/FEXCore/IR/IRDefines.inc, ../../../include/FEXCore/IR/IRDefines_Dispatch.inc
[97/189] Generating ../../../include/FEXCore/Config/ConfigValues.inl, ../../../include/FEXCore/Config/ConfigOptions.inl, ../../../generated/FEX.1
[98/189] Generating ../../../generated/FEX.1.gz
[99/189] Building CXX object fex/External/fmt/CMakeFiles/fmt.dir/src/format.cc.o
[100/189] Building CXX object CMakeFiles/px5.dir/core/emulator.cpp.o
[101/189] Building CXX object CMakeFiles/px5.dir/fexcore_wrapper.cpp.o
[102/189] Building CXX object CMakeFiles/px5.dir/fexcore_integration.cpp.o
[103/189] Building CXX object CMakeFiles/px5.dir/kernel/signals.cpp.o
[104/189] Building CXX object CMakeFiles/px5.dir/memory/memory.cpp.o
[105/189] Building CXX object CMakeFiles/px5.dir/memory/memory_map.cpp.o
[106/189] Building CXX object CMakeFiles/px5.dir/kernel/syscalls.cpp.o
[107/189] Building CXX object CMakeFiles/px5.dir/kernel_hle.cpp.o
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:63:100: warning: format specifies type 'unsigned long long' but the argument has type 'uint64_t' (aka 'unsigned long') [-Wformat]
   63 |             LOGI("Kernel HLE: Intercepted Syscall #%u (args: 0x%llx, 0x%llx, 0x%llx)", syscallNum, arg1, arg2, arg3);
      |                                                                ~~~~                                ^~~~
      |                                                                %lx
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:6:66: note: expanded from macro 'LOGI'
    6 | #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
      |                                                                  ^~~~~~~~~~~
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:63:106: warning: format specifies type 'unsigned long long' but the argument has type 'uint64_t' (aka 'unsigned long') [-Wformat]
   63 |             LOGI("Kernel HLE: Intercepted Syscall #%u (args: 0x%llx, 0x%llx, 0x%llx)", syscallNum, arg1, arg2, arg3);
      |                                                                        ~~~~                              ^~~~
      |                                                                        %lx
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:6:66: note: expanded from macro 'LOGI'
    6 | #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
      |                                                                  ^~~~~~~~~~~
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:63:112: warning: format specifies type 'unsigned long long' but the argument has type 'uint64_t' (aka 'unsigned long') [-Wformat]
   63 |             LOGI("Kernel HLE: Intercepted Syscall #%u (args: 0x%llx, 0x%llx, 0x%llx)", syscallNum, arg1, arg2, arg3);
      |                                                                                ~~~~                            ^~~~
      |                                                                                %lx
/home/runner/work/PSX5/PSX5/app/src/main/cpp/kernel_hle.cpp:6:66: note: expanded from macro 'LOGI'
    6 | #define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
      |                                                                  ^~~~~~~~~~~
3 warnings generated.
[108/189] Building CXX object CMakeFiles/px5.dir/audio/audio.cpp.o
[109/189] Building CXX object CMakeFiles/px5.dir/gpu/vulkan_device.cpp.o
[110/189] Building CXX object CMakeFiles/px5.dir/loader/elf_loader.cpp.o
[111/189] Building CXX object CMakeFiles/px5.dir/gpu/shader_cache.cpp.o
[112/189] Building CXX object CMakeFiles/px5.dir/loader/self_loader.cpp.o
[113/189] Building CXX object CMakeFiles/px5.dir/gnm_vulkan_renderer.cpp.o
[114/189] Building CXX object CMakeFiles/px5.dir/filesystem/vfs.cpp.o
[115/189] Building CXX object CMakeFiles/px5.dir/audio_input_native.cpp.o
[116/189] Building CXX object CMakeFiles/px5.dir/input/controller.cpp.o
[117/189] Building CXX object CMakeFiles/px5.dir/turnip_hook.cpp.o
[118/189] Building CXX object CMakeFiles/px5.dir/utils/logger.cpp.o
[119/189] Building CXX object CMakeFiles/px5.dir/utils/linker_ns_bypass.cpp.o
[120/189] Linking CXX static library fex/External/fmt/libfmtd.a
[121/189] Building CXX object CMakeFiles/px5.dir/utils/crash_handler.cpp.o
FAILED: CMakeFiles/px5.dir/utils/crash_handler.cpp.o 
/usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ --target=aarch64-none-linux-android28 --sysroot=/usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/sysroot -DFEXCORE_HAS_PRESERVE_ALL_ATTR=1 -DFEXCORE_PRESERVE_ALL_ATTR="__attribute__((preserve_all))" -DHAS_SYSCALL_GETTID=1 -DHAS_SYSCALL_TGKILL=1 -DINLINE="static inline" -DINLINE_LEVEL=4 -DSOFTFLOAT_BUILTIN_CLZ=1 -DSOFTFLOAT_FAST_DIV32TO16=1 -DSOFTFLOAT_FAST_DIV64TO32=1 -DSOFTFLOAT_FAST_INT64=1 -Dpx5_EXPORTS -I/home/runner/work/PSX5/PSX5/app/src/main/cpp/../../../../third_party/fex/FEXCore/include -I/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/fex/FEXCore/include -I/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/fex/generated -I/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/fex/FEXCore/Source -I/home/runner/work/PSX5/PSX5/third_party/fex/FEXCore/include -I/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/unordered_dense/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/fmt/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/xxhash/cmake_unofficial/.. -I/home/runner/work/PSX5/PSX5/third_party/fex/FEXHeaderUtils/. -I/home/runner/work/PSX5/PSX5/third_party/fex/CodeEmitter/. -I/home/runner/work/PSX5/PSX5/third_party/fex/External/cephes/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/SoftFloat-3e/include -I/home/runner/work/PSX5/PSX5/third_party/fex/External/SoftFloat-3e/include/SoftFloat-3e -g -DANDROID -fdata-sections -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -Wformat -Werror=format-security   -fno-limit-debug-info  -fPIC -include /home/runner/work/PSX5/PSX5/app/src/main/cpp/compat/std_atomic_ref_polyfill.h -std=c++20 -MD -MT CMakeFiles/px5.dir/utils/crash_handler.cpp.o -MF CMakeFiles/px5.dir/utils/crash_handler.cpp.o.d -o CMakeFiles/px5.dir/utils/crash_handler.cpp.o -c /home/runner/work/PSX5/PSX5/app/src/main/cpp/utils/crash_handler.cpp
/home/runner/work/PSX5/PSX5/app/src/main/cpp/utils/crash_handler.cpp:173:15: error: reinterpret_cast from '_Unwind_Word' (aka 'unsigned long') to 'unsigned long long' is not allowed
  173 |     auto pc = reinterpret_cast<unsigned long long>(_Unwind_GetIP(ctx));
      |               ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1 error generated.
[122/189] Building CXX object CMakeFiles/px5.dir/media/media_codec_hle.cpp.o
[123/189] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Utils/FileLoading.cpp.o
ninja: build stopped: subcommand failed.

C++ build system [build] failed while executing:
    /usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja \
      -C \
      /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a \
      px5
  from /home/runner/work/PSX5/PSX5/app
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt.execute(ExecuteProcess.kt:288)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt$executeProcess$1.invoke(ExecuteProcess.kt:108)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt$executeProcess$1.invoke(ExecuteProcess.kt:106)
	at com.android.build.gradle.internal.cxx.timing.TimingEnvironmentKt.time(TimingEnvironment.kt:32)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt.executeProcess(ExecuteProcess.kt:106)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt.executeProcess$default(ExecuteProcess.kt:85)
	at com.android.build.gradle.internal.cxx.build.CxxRegularBuilder.executeProcessBatch(CxxRegularBuilder.kt:332)
	at com.android.build.gradle.internal.cxx.build.CxxRegularBuilder.build(CxxRegularBuilder.kt:129)
	at com.android.build.gradle.tasks.ExternalNativeBuildTask.doTaskAction(ExternalNativeBuildTask.kt:72)
	at com.android.build.gradle.internal.tasks.UnsafeOutputsTask$taskAction$$inlined$recordTaskAction$1.invoke(BaseTask.kt:78)
	at com.android.build.gradle.internal.tasks.Blocks.recordSpan(Blocks.java:51)
	at com.android.build.gradle.internal.tasks.UnsafeOutputsTask.taskAction(UnsafeOutputsTask.kt:81)
	at java.base/jdk.internal.reflect.NativeMethodAccessorImpl.invoke0(Native Method)
	at java.base/jdk.internal.reflect.NativeMethodAccessorImpl.invoke(NativeMethodAccessorImpl.java:77)
	at java.base/jdk.internal.reflect.DelegatingMethodAccessorImpl.invoke(DelegatingMethodAccessorImpl.java:43)
	at org.gradle.internal.reflect.JavaMethod.invoke(JavaMethod.java:125)
	... 116 more
Caused by: com.android.ide.common.process.ProcessException: Error while executing process /usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja with arguments {-C /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a px5}
	at com.android.build.gradle.internal.process.GradleProcessResult.buildProcessException(GradleProcessResult.java:73)
	at com.android.build.gradle.internal.process.GradleProcessResult.assertNormalExitValue(GradleProcessResult.java:48)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt.execute(ExecuteProcess.kt:277)
	... 131 more
Caused by: org.gradle.process.internal.ExecException: Process 'command '/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja'' finished with non-zero exit value 1
	at org.gradle.process.internal.DefaultExecHandle$ExecResultImpl.assertNormalExitValue(DefaultExecHandle.java:442)
	at com.android.build.gradle.internal.process.GradleProcessResult.assertNormalExitValue(GradleProcessResult.java:46)
	... 132 more


BUILD FAILED in 4m 34s
25 actionable tasks: 25 executed
```
