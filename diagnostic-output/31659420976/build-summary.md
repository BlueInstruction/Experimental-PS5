# Build Summary

- gradle exit code: 1

## Matched failure patterns
```
325:Resource missing. [HTTP GET: https://dl.google.com/dl/android/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.pom]
329:Downloading https://repo.maven.apache.org/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.pom to /home/runner/.gradle/.tmp/gradle_download7497849877048021425bin
482:Downloading https://repo.maven.apache.org/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.jar to /home/runner/.gradle/.tmp/gradle_download12978356675402742017bin
632:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with InstrumentationAnalysisTransform
1050:Caching disabled for InstrumentationAnalysisTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
1363:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with InstrumentationAnalysisTransform
1364:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with MergeInstrumentationAnalysisTransform
3524:Caching disabled for MergeInstrumentationAnalysisTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
3527:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with ExternalDependencyInstrumentingArtifactTransform
3531:Caching disabled for ExternalDependencyInstrumentingArtifactTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
9071:C/C++: TypeError: argument of type 'NoneType' is not iterable
9226:Full recompilation is required because no incremental change information is available. This is usually caused by clean builds or changing compiler arguments.
10069:> Task :app:configureCMakeDebug[armeabi-v7a] FAILED
10118:C/C++: CMake Error at /home/runner/work/PSX5/PSX5/third_party/fex/CMakeLists.txt:61 (message):
10119:C/C++:   Unsupported pointer size 4.  FEX only supports 64-bit (8-byte pointer)
10161:CMake Error at /home/runner/work/PSX5/PSX5/third_party/fex/CMakeLists.txt:61 (message):
10162:  Unsupported pointer size 4.  FEX only supports 64-bit (8-byte pointer)
10166:FAILURE: Build failed with an exception.
10209:  CMake Error at /home/runner/work/PSX5/PSX5/third_party/fex/CMakeLists.txt:61 (message):
10210:    Unsupported pointer size 4.  FEX only supports 64-bit (8-byte pointer)
10250:  CMake Error at /home/runner/work/PSX5/PSX5/third_party/fex/CMakeLists.txt:61 (message):
10251:    Unsupported pointer size 4.  FEX only supports 64-bit (8-byte pointer)
10421:  	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
10426:  Caused by: com.android.ide.common.process.ProcessException: Error while executing process /usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake with arguments {-H/home/runner/work/PSX5/PSX5/app/src/main/cpp -DCMAKE_SYSTEM_NAME=Android -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_SYSTEM_VERSION=28 -DANDROID_PLATFORM=android-28 -DANDROID_ABI=armeabi-v7a -DCMAKE_ANDROID_ARCH_ABI=armeabi-v7a -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a -DCMAKE_BUILD_TYPE=Debug -B/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/armeabi-v7a -GNinja}
10431:  Caused by: org.gradle.process.internal.ExecException: Process 'command '/usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake'' finished with non-zero exit value 1
10445:	at org.gradle.internal.Try$Failure.ifSuccessfulOrElse(Try.java:293)
10502:	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
10504:Caused by: com.android.builder.errors.EvalIssueException: [CXX1429] error when building with cmake using /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt: -- The C compiler identification is Clang 18.0.4
10543:CMake Error at /home/runner/work/PSX5/PSX5/third_party/fex/CMakeLists.txt:61 (message):
10544:  Unsupported pointer size 4.  FEX only supports 64-bit (8-byte pointer)
10584:CMake Error at /home/runner/work/PSX5/PSX5/third_party/fex/CMakeLists.txt:61 (message):
10585:  Unsupported pointer size 4.  FEX only supports 64-bit (8-byte pointer)
10755:	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
10760:Caused by: com.android.ide.common.process.ProcessException: Error while executing process /usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake with arguments {-H/home/runner/work/PSX5/PSX5/app/src/main/cpp -DCMAKE_SYSTEM_NAME=Android -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_SYSTEM_VERSION=28 -DANDROID_PLATFORM=android-28 -DANDROID_ABI=armeabi-v7a -DCMAKE_ANDROID_ARCH_ABI=armeabi-v7a -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a -DCMAKE_BUILD_TYPE=Debug -B/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/armeabi-v7a -GNinja}
10765:Caused by: org.gradle.process.internal.ExecException: Process 'command '/usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake'' finished with non-zero exit value 1
10927:	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
10931:BUILD FAILED in 6m 32s
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
3770:Tasks to be executed: [task ':app:preBuild', task ':app:preDebugBuild', task ':app:mergeDebugNativeDebugMetadata', task ':app:checkKotlinGradlePluginConfigurationErrors', task ':app:checkDebugAarMetadata', task ':app:generateDebugResValues', task ':app:mapDebugSourceSetPaths', task ':app:generateDebugResources', task ':app:mergeDebugResources', task ':app:packageDebugResources', task ':app:parseDebugLocalResources', task ':app:createDebugCompatibleScreenManifests', task ':app:extractDeepLinksDebug', task ':app:processDebugMainManifest', task ':app:processDebugManifest', task ':app:processDebugManifestForPackage', task ':app:processDebugResources', task ':app:kspDebugKotlin', task ':app:compileDebugKotlin', task ':app:javaPreCompileDebug', task ':app:compileDebugJavaWithJavac', task ':app:mergeDebugShaders', task ':app:compileDebugShaders', task ':app:generateDebugAssets', task ':app:mergeDebugAssets', task ':app:compressDebugAssets', task ':app:desugarDebugFileDependencies', task ':app:dexBuilderDebug', task ':app:mergeDebugGlobalSynthetics', task ':app:processDebugJavaRes', task ':app:mergeDebugJavaResource', task ':app:checkDebugDuplicateClasses', task ':app:mergeExtDexDebug', task ':app:mergeLibDexDebug', task ':app:mergeProjectDexDebug', task ':app:configureCMakeDebug[arm64-v8a]', task ':app:buildCMakeDebug[arm64-v8a][px5]', task ':app:configureCMakeDebug[armeabi-v7a]', task ':app:buildCMakeDebug[armeabi-v7a][px5]', task ':app:configureCMakeDebug[x86]', task ':app:buildCMakeDebug[x86][px5]', task ':app:configureCMakeDebug[x86_64]', task ':app:buildCMakeDebug[x86_64][px5]', task ':app:mergeDebugJniLibFolders', task ':app:mergeDebugNativeLibs', task ':app:stripDebugDebugSymbols', task ':app:validateSigningDebug', task ':app:writeDebugAppMetadata', task ':app:writeDebugSigningConfigVersions', task ':app:packageDebug', task ':app:createDebugApkListingFileRedirect', task ':app:assembleDebug']
8814:INFO: D8: Some warnings are typically a sign of using an outdated Java toolchain. To fix, recompile the source with an updated toolchain.
8857:Resolve mutations for :app:configureCMakeDebug[arm64-v8a] (Thread[included builds,5,main]) started.
8858::app:configureCMakeDebug[arm64-v8a] (Thread[included builds,5,main]) started.
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
      -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
      -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
      -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake \
      -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja \
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a \
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a \
      -DCMAKE_BUILD_TYPE=Debug \
      -B/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/armeabi-v7a \
      -GNinja
  from /home/runner/work/PSX5/PSX5/app
CMake Error at /home/runner/work/PSX5/PSX5/third_party/fex/CMakeLists.txt:61 (message):
  Unsupported pointer size 4.  FEX only supports 64-bit (8-byte pointer)
  systems.  If you believe this is in error, file an issue. : com.android.ide.common.process.ProcessException: -- The C compiler identification is Clang 18.0.4
-- The CXX compiler identification is Clang 18.0.4
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/clang - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- The ASM compiler identification is Clang with GNU-like command-line
-- Found assembler: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/clang
-- Looking for include file gdb/jit-reader.h
-- Looking for include file gdb/jit-reader.h - not found
-- Configuring incomplete, errors occurred!
See also "/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/armeabi-v7a/CMakeFiles/CMakeOutput.log".
See also "/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/armeabi-v7a/CMakeFiles/CMakeError.log".

C++ build system [configure] failed while executing:
    /usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake \
      -H/home/runner/work/PSX5/PSX5/app/src/main/cpp \
      -DCMAKE_SYSTEM_NAME=Android \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_SYSTEM_VERSION=28 \
      -DANDROID_PLATFORM=android-28 \
      -DANDROID_ABI=armeabi-v7a \
      -DCMAKE_ANDROID_ARCH_ABI=armeabi-v7a \
      -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
      -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
      -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake \
      -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja \
      -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a \
      -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a \
      -DCMAKE_BUILD_TYPE=Debug \
      -B/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/armeabi-v7a \
      -GNinja
  from /home/runner/work/PSX5/PSX5/app
CMake Error at /home/runner/work/PSX5/PSX5/third_party/fex/CMakeLists.txt:61 (message):
  Unsupported pointer size 4.  FEX only supports 64-bit (8-byte pointer)
  systems.  If you believe this is in error, file an issue.
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt.execute(ExecuteProcess.kt:288)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt$executeProcess$1.invoke(ExecuteProcess.kt:108)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt$executeProcess$1.invoke(ExecuteProcess.kt:106)
	at com.android.build.gradle.internal.cxx.timing.TimingEnvironmentKt.time(TimingEnvironment.kt:32)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt.executeProcess(ExecuteProcess.kt:106)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt.executeProcess$default(ExecuteProcess.kt:85)
	at com.android.build.gradle.tasks.CmakeQueryMetadataGenerator.executeProcess(CmakeFileApiMetadataGenerator.kt:59)
	at com.android.build.gradle.tasks.ExternalNativeJsonGenerator$configureOneAbi$1$1$3.invoke(ExternalNativeJsonGenerator.kt:247)
	at com.android.build.gradle.tasks.ExternalNativeJsonGenerator$configureOneAbi$1$1$3.invoke(ExternalNativeJsonGenerator.kt:247)
	at com.android.build.gradle.internal.cxx.timing.TimingEnvironmentKt.time(TimingEnvironment.kt:32)
	at com.android.build.gradle.tasks.ExternalNativeJsonGenerator.configureOneAbi(ExternalNativeJsonGenerator.kt:247)
	at com.android.build.gradle.tasks.ExternalNativeJsonGenerator.configure(ExternalNativeJsonGenerator.kt:113)
	at com.android.build.gradle.tasks.ExternalNativeBuildJsonTask.doTaskAction(ExternalNativeBuildJsonTask.kt:89)
	at com.android.build.gradle.internal.tasks.UnsafeOutputsTask$taskAction$$inlined$recordTaskAction$1.invoke(BaseTask.kt:78)
	at com.android.build.gradle.internal.tasks.Blocks.recordSpan(Blocks.java:51)
	at com.android.build.gradle.internal.tasks.UnsafeOutputsTask.taskAction(UnsafeOutputsTask.kt:81)
	at java.base/jdk.internal.reflect.NativeMethodAccessorImpl.invoke0(Native Method)
	at java.base/jdk.internal.reflect.NativeMethodAccessorImpl.invoke(NativeMethodAccessorImpl.java:77)
	at java.base/jdk.internal.reflect.DelegatingMethodAccessorImpl.invoke(DelegatingMethodAccessorImpl.java:43)
	at java.base/java.lang.reflect.Method.invoke(Method.java:569)
	at org.gradle.internal.reflect.JavaMethod.invoke(JavaMethod.java:125)
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
	at java.base/java.util.Optional.orElseGet(Optional.java:364)
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
	at java.base/java.util.Optional.orElseGet(Optional.java:364)
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
	at org.gradle.execution.plan.DefaultPlanExecutor.process(DefaultPlanExecutor.java:111)
	at org.gradle.execution.taskgraph.DefaultTaskExecutionGraph.executeWithServices(DefaultTaskExecutionGraph.java:138)
	at org.gradle.execution.taskgraph.DefaultTaskExecutionGraph.execute(DefaultTaskExecutionGraph.java:123)
	at org.gradle.execution.SelectedTaskExecutionAction.execute(SelectedTaskExecutionAction.java:35)
	at org.gradle.execution.DryRunBuildExecutionAction.execute(DryRunBuildExecutionAction.java:51)
	at org.gradle.execution.BuildOperationFiringBuildWorkerExecutor$ExecuteTasks.call(BuildOperationFiringBuildWorkerExecutor.java:54)
	at org.gradle.execution.BuildOperationFiringBuildWorkerExecutor$ExecuteTasks.call(BuildOperationFiringBuildWorkerExecutor.java:43)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:209)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:204)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:66)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:166)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.call(DefaultBuildOperationRunner.java:53)
	at org.gradle.execution.BuildOperationFiringBuildWorkerExecutor.execute(BuildOperationFiringBuildWorkerExecutor.java:40)
	at org.gradle.internal.build.DefaultBuildLifecycleController.lambda$executeTasks$10(DefaultBuildLifecycleController.java:313)
	at org.gradle.internal.model.StateTransitionController.doTransition(StateTransitionController.java:266)
	at org.gradle.internal.model.StateTransitionController.lambda$tryTransition$8(StateTransitionController.java:177)
	at org.gradle.internal.work.DefaultSynchronizer.withLock(DefaultSynchronizer.java:44)
	at org.gradle.internal.model.StateTransitionController.tryTransition(StateTransitionController.java:177)
	at org.gradle.internal.build.DefaultBuildLifecycleController.executeTasks(DefaultBuildLifecycleController.java:304)
	at org.gradle.internal.build.DefaultBuildWorkGraphController$DefaultBuildWorkGraph.runWork(DefaultBuildWorkGraphController.java:220)
	at org.gradle.internal.work.DefaultWorkerLeaseService.withLocks(DefaultWorkerLeaseService.java:267)
	at org.gradle.internal.work.DefaultWorkerLeaseService.runAsWorkerThread(DefaultWorkerLeaseService.java:131)
	at org.gradle.composite.internal.DefaultBuildController.doRun(DefaultBuildController.java:181)
	at org.gradle.composite.internal.DefaultBuildController.access$000(DefaultBuildController.java:50)
	at org.gradle.composite.internal.DefaultBuildController$BuildOpRunnable.lambda$run$0(DefaultBuildController.java:198)
	at org.gradle.internal.operations.CurrentBuildOperationRef.with(CurrentBuildOperationRef.java:85)
	at org.gradle.composite.internal.DefaultBuildController$BuildOpRunnable.run(DefaultBuildController.java:198)
	at java.base/java.util.concurrent.Executors$RunnableAdapter.call(Executors.java:539)
	at java.base/java.util.concurrent.FutureTask.run(FutureTask.java:264)
	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
	at org.gradle.internal.concurrent.AbstractManagedExecutor$1.run(AbstractManagedExecutor.java:48)
	at java.base/java.util.concurrent.ThreadPoolExecutor.runWorker(ThreadPoolExecutor.java:1136)
	at java.base/java.util.concurrent.ThreadPoolExecutor$Worker.run(ThreadPoolExecutor.java:635)
	at java.base/java.lang.Thread.run(Thread.java:840)
Caused by: com.android.ide.common.process.ProcessException: Error while executing process /usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake with arguments {-H/home/runner/work/PSX5/PSX5/app/src/main/cpp -DCMAKE_SYSTEM_NAME=Android -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_SYSTEM_VERSION=28 -DANDROID_PLATFORM=android-28 -DANDROID_ABI=armeabi-v7a -DCMAKE_ANDROID_ARCH_ABI=armeabi-v7a -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/armeabi-v7a -DCMAKE_BUILD_TYPE=Debug -B/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/armeabi-v7a -GNinja}
	at com.android.build.gradle.internal.process.GradleProcessResult.buildProcessException(GradleProcessResult.java:73)
	at com.android.build.gradle.internal.process.GradleProcessResult.assertNormalExitValue(GradleProcessResult.java:48)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt.execute(ExecuteProcess.kt:277)
	... 172 more
Caused by: org.gradle.process.internal.ExecException: Process 'command '/usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake'' finished with non-zero exit value 1
	at org.gradle.process.internal.DefaultExecHandle$ExecResultImpl.assertNormalExitValue(DefaultExecHandle.java:442)
	at com.android.build.gradle.internal.process.GradleProcessResult.assertNormalExitValue(GradleProcessResult.java:46)
	... 173 more

	at com.android.builder.errors.IssueReporter.reportError(IssueReporter.kt:118)
	at com.android.builder.errors.IssueReporter.reportError$default(IssueReporter.kt:114)
	at com.android.build.gradle.internal.cxx.logging.IssueReporterLoggingEnvironment.log(IssueReporterLoggingEnvironment.kt:113)
	at com.android.build.gradle.internal.cxx.logging.ThreadLoggingEnvironment$Companion.reportFormattedErrorToCurrentLogger(LoggingEnvironment.kt:230)
	at com.android.build.gradle.internal.cxx.logging.LoggingEnvironmentKt.errorln(LoggingEnvironment.kt:62)
	at com.android.build.gradle.tasks.ExternalNativeJsonGenerator.configure(ExternalNativeJsonGenerator.kt:121)
	at com.android.build.gradle.tasks.ExternalNativeBuildJsonTask.doTaskAction(ExternalNativeBuildJsonTask.kt:89)
	at com.android.build.gradle.internal.tasks.UnsafeOutputsTask$taskAction$$inlined$recordTaskAction$1.invoke(BaseTask.kt:78)
	at com.android.build.gradle.internal.tasks.Blocks.recordSpan(Blocks.java:51)
	at com.android.build.gradle.internal.tasks.UnsafeOutputsTask.taskAction(UnsafeOutputsTask.kt:81)
	at java.base/jdk.internal.reflect.NativeMethodAccessorImpl.invoke0(Native Method)
	at java.base/jdk.internal.reflect.NativeMethodAccessorImpl.invoke(NativeMethodAccessorImpl.java:77)
	at java.base/jdk.internal.reflect.DelegatingMethodAccessorImpl.invoke(DelegatingMethodAccessorImpl.java:43)
	at org.gradle.internal.reflect.JavaMethod.invoke(JavaMethod.java:125)
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
	at org.gradle.execution.plan.DefaultPlanExecutor.process(DefaultPlanExecutor.java:111)
	at org.gradle.execution.taskgraph.DefaultTaskExecutionGraph.executeWithServices(DefaultTaskExecutionGraph.java:138)
	at org.gradle.execution.taskgraph.DefaultTaskExecutionGraph.execute(DefaultTaskExecutionGraph.java:123)
	at org.gradle.execution.SelectedTaskExecutionAction.execute(SelectedTaskExecutionAction.java:35)
	at org.gradle.execution.DryRunBuildExecutionAction.execute(DryRunBuildExecutionAction.java:51)
	at org.gradle.execution.BuildOperationFiringBuildWorkerExecutor$ExecuteTasks.call(BuildOperationFiringBuildWorkerExecutor.java:54)
	at org.gradle.execution.BuildOperationFiringBuildWorkerExecutor$ExecuteTasks.call(BuildOperationFiringBuildWorkerExecutor.java:43)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:209)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:204)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:66)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:166)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.call(DefaultBuildOperationRunner.java:53)
	at org.gradle.execution.BuildOperationFiringBuildWorkerExecutor.execute(BuildOperationFiringBuildWorkerExecutor.java:40)
	at org.gradle.internal.build.DefaultBuildLifecycleController.lambda$executeTasks$10(DefaultBuildLifecycleController.java:313)
	at org.gradle.internal.model.StateTransitionController.doTransition(StateTransitionController.java:266)
	at org.gradle.internal.model.StateTransitionController.lambda$tryTransition$8(StateTransitionController.java:177)
	at org.gradle.internal.work.DefaultSynchronizer.withLock(DefaultSynchronizer.java:44)
	at org.gradle.internal.model.StateTransitionController.tryTransition(StateTransitionController.java:177)
	at org.gradle.internal.build.DefaultBuildLifecycleController.executeTasks(DefaultBuildLifecycleController.java:304)
	at org.gradle.internal.build.DefaultBuildWorkGraphController$DefaultBuildWorkGraph.runWork(DefaultBuildWorkGraphController.java:220)
	at org.gradle.internal.work.DefaultWorkerLeaseService.withLocks(DefaultWorkerLeaseService.java:267)
	at org.gradle.internal.work.DefaultWorkerLeaseService.runAsWorkerThread(DefaultWorkerLeaseService.java:131)
	at org.gradle.composite.internal.DefaultBuildController.doRun(DefaultBuildController.java:181)
	at org.gradle.composite.internal.DefaultBuildController.access$000(DefaultBuildController.java:50)
	at org.gradle.composite.internal.DefaultBuildController$BuildOpRunnable.lambda$run$0(DefaultBuildController.java:198)
	at org.gradle.internal.operations.CurrentBuildOperationRef.with(CurrentBuildOperationRef.java:85)
	at org.gradle.composite.internal.DefaultBuildController$BuildOpRunnable.run(DefaultBuildController.java:198)
	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
	at org.gradle.internal.concurrent.AbstractManagedExecutor$1.run(AbstractManagedExecutor.java:48)


BUILD FAILED in 6m 32s
32 actionable tasks: 32 executed
```
