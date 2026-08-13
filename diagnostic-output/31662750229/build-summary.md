# Build Summary

- gradle exit code: 1

## Matched failure patterns
```
325:Resource missing. [HTTP GET: https://dl.google.com/dl/android/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.pom]
327:Downloading https://repo.maven.apache.org/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.pom to /home/runner/.gradle/.tmp/gradle_download8498141703624697001bin
483:Downloading https://repo.maven.apache.org/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.jar to /home/runner/.gradle/.tmp/gradle_download10237354586214173311bin
632:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with InstrumentationAnalysisTransform
1050:Caching disabled for InstrumentationAnalysisTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
1363:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with InstrumentationAnalysisTransform
1364:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with MergeInstrumentationAnalysisTransform
3524:Caching disabled for MergeInstrumentationAnalysisTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
3527:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with ExternalDependencyInstrumentingArtifactTransform
3531:Caching disabled for ExternalDependencyInstrumentingArtifactTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
9057:> Task :app:compileDebugKotlin FAILED
9087:C/C++: TypeError: argument of type 'NoneType' is not iterable
9096:FAILURE: Build failed with an exception.
9100:> A failure occurred while executing org.jetbrains.kotlin.compilerRunner.GradleCompilerRunnerWithWorkers$GradleKotlinCompilerWorkAction
9111:	at org.gradle.internal.Try$Failure.ifSuccessfulOrElse(Try.java:293)
9139:	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
9141:Caused by: org.gradle.workers.internal.DefaultWorkerExecutor$WorkExecutionException: A failure occurred while executing org.jetbrains.kotlin.compilerRunner.GradleCompilerRunnerWithWorkers$GradleKotlinCompilerWorkAction
9143:	at org.gradle.internal.work.DefaultAsyncWorkTracker.lambda$waitForItemsAndGatherFailures$2(DefaultAsyncWorkTracker.java:130)
9148:	at org.gradle.internal.work.DefaultAsyncWorkTracker.waitForItemsAndGatherFailures(DefaultAsyncWorkTracker.java:126)
9149:	at org.gradle.internal.work.DefaultAsyncWorkTracker.waitForItemsAndGatherFailures(DefaultAsyncWorkTracker.java:92)
9263:	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
9265:Caused by: org.jetbrains.kotlin.gradle.tasks.CompilationErrorException: Compilation error. See log for more details
9298:BUILD FAILED in 3m 48s
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
8786:INFO: D8: Some warnings are typically a sign of using an outdated Java toolchain. To fix, recompile the source with an updated toolchain.
8972:Resolve mutations for :app:configureCMakeDebug[arm64-v8a] (Thread[included builds,5,main]) started.
8973::app:configureCMakeDebug[arm64-v8a] (Thread[included builds,5,main]) started.
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
Transforming kotlinx-coroutines-core-jvm-1.7.3.jar with BuildToolsApiClasspathEntrySnapshotTransform
Transforming kotlinx-coroutines-android-1.7.3.jar with BuildToolsApiClasspathEntrySnapshotTransform
Transforming kotlin-stdlib-jdk8-1.8.22.jar with BuildToolsApiClasspathEntrySnapshotTransform
Transforming kotlin-stdlib-jdk7-1.8.22.jar with BuildToolsApiClasspathEntrySnapshotTransform
Transforming kotlin-stdlib-2.0.21.jar with BuildToolsApiClasspathEntrySnapshotTransform
Transforming annotations-23.0.0.jar with BuildToolsApiClasspathEntrySnapshotTransform
Transforming android.jar with BuildToolsApiClasspathEntrySnapshotTransform
Transforming core-lambda-stubs.jar with BuildToolsApiClasspathEntrySnapshotTransform
Resource missing. [HTTP GET: https://dl.google.com/dl/android/maven2/org/jetbrains/kotlin/kotlin-compose-compiler-plugin-embeddable/2.0.21/kotlin-compose-compiler-plugin-embeddable-2.0.21.pom]
Downloading https://repo.maven.apache.org/maven2/org/jetbrains/kotlin/kotlin-compose-compiler-plugin-embeddable/2.0.21/kotlin-compose-compiler-plugin-embeddable-2.0.21.pom to /home/runner/.gradle/.tmp/gradle_download6220109061078976435bin
Downloading https://repo.maven.apache.org/maven2/org/jetbrains/kotlin/kotlin-compose-compiler-plugin-embeddable/2.0.21/kotlin-compose-compiler-plugin-embeddable-2.0.21.jar to /home/runner/.gradle/.tmp/gradle_download10584738344406272786bin
Caching disabled for task ':app:compileDebugKotlin' because:
  Build cache is disabled
Task ':app:compileDebugKotlin' is not up-to-date because:
  No history is available.
The input changes require a full rebuild for incremental task ':app:compileDebugKotlin'.
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/main/kotlin', not found
file or directory '/home/runner/work/PSX5/PSX5/app/src/debug/java', not found
Kotlin source files: /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/GameDatabase.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/GameViewModel.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/SoundManager.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5Theme.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5Notifications.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SearchScreen.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5ControlCenter.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5TurnipDriverDialog.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5HomeScreen.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5StoreScreen.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5PkgInstallerDialog.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5Components.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/PX5Application.kt, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/MainActivity.kt
Java source files: /home/runner/work/PSX5/PSX5/app/build/generated/ksp/debug/java/com/px5/emulator/AppDatabase_Impl.java, /home/runner/work/PSX5/PSX5/app/build/generated/ksp/debug/java/com/px5/emulator/GameDao_Impl.java, /home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/core/FexCoreWrapper.java
Script source files: 
Script file extensions: 
Using Kotlin/JVM incremental compilation
[KOTLIN] Kotlin compilation 'jdkHome' argument: null
Options for KOTLIN DAEMON: IncrementalCompilationOptions(super=CompilationOptions(compilerMode=INCREMENTAL_COMPILER, targetPlatform=JVM, reportCategories=[0, 3], reportSeverity=2, requestedCompilationResults=[0], kotlinScriptExtensions=[]), areFileChangesKnown=false, modifiedFiles=null, deletedFiles=null, classpathChanges=NotAvailableForNonIncrementalRun, workingDir=/home/runner/work/PSX5/PSX5/app/build/kotlin/compileDebugKotlin/cacheable, multiModuleICSettings=MultiModuleICSettings(buildHistoryFile=/home/runner/work/PSX5/PSX5/app/build/kotlin/compileDebugKotlin/local-state/build-history.bin, useModuleDetection=true), usePreciseJavaTracking=true, icFeatures=IncrementalCompilationFeatures(withAbiSnapshot=false, preciseCompilationResultsBackup=true, keepIncrementalCompilationCachesInMemory=true, enableUnsafeIncrementalCompilationForMultiplatform=false), outputFiles=[/home/runner/work/PSX5/PSX5/app/build/tmp/kotlin-classes/debug, /home/runner/work/PSX5/PSX5/app/build/kotlin/compileDebugKotlin/cacheable, /home/runner/work/PSX5/PSX5/app/build/kotlin/compileDebugKotlin/local-state])

> Task :app:mergeLibDexDebug
Caching disabled for task ':app:mergeLibDexDebug' because:
  Build cache is disabled
Task ':app:mergeLibDexDebug' is not up-to-date because:
  No history is available.
The input changes require a full rebuild for incremental task ':app:mergeLibDexDebug'.
Resolve mutations for :app:configureCMakeDebug[arm64-v8a] (Thread[included builds,5,main]) started.
:app:configureCMakeDebug[arm64-v8a] (Thread[included builds,5,main]) started.

> Task :app:configureCMakeDebug[arm64-v8a]
Caching disabled for task ':app:configureCMakeDebug[arm64-v8a]' because:
  Build cache is disabled
  Caching has been disabled for the task
Task ':app:configureCMakeDebug[arm64-v8a]' is not up-to-date because:
  Task.upToDateWhen is false.
C/C++: Parsing /usr/local/lib/android/sdk/build-tools/34.0.0/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/build-tools/35.0.0/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/build-tools/35.0.1/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/build-tools/36.0.0/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/build-tools/36.1.0/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/build-tools/37.0.0/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/cmake/3.22.1/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/cmake/3.31.5/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/cmake/4.1.2/package.xml
C/C++: Parsing legacy package: /usr/local/lib/android/sdk/cmdline-tools/16.0
C/C++: Parsing legacy package: /usr/local/lib/android/sdk/cmdline-tools/latest
C/C++: Parsing /usr/local/lib/android/sdk/extras/android/m2repository/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/extras/google/google_play_services/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/extras/google/m2repository/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/ndk/27.3.13750724/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/ndk/28.2.13676358/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/ndk/29.0.14206865/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platform-tools/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-34/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-34-ext10/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-34-ext11/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-34-ext12/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-34-ext8/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-35/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-35-ext14/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-35-ext15/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-36/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-36-ext18/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-36-ext19/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-36.1/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-37.0/package.xml
C/C++: Parsing /usr/local/lib/android/sdk/platforms/android-37.1/package.xml
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : Start JSON generation. Platform version: 28 min SDK version: arm64-v8a
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : rebuilding JSON /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/android_gradle_build.json due to:
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : - no fingerprint file, will remove stale configuration folder
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : removing stale contents from '/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : created folder '/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : executing cmake /usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake \
  -H/home/runner/work/PSX5/PSX5/app/src/main/cpp \
  -DCMAKE_SYSTEM_NAME=Android \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_SYSTEM_VERSION=28 \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_ABI=arm64-v8a \
  -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
  -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
  -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
  -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake \
  -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja \
  -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a \
  -DCMAKE_BUILD_TYPE=Debug \
  -B/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a \
  -GNinja

C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : /usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake \
  -H/home/runner/work/PSX5/PSX5/app/src/main/cpp \
  -DCMAKE_SYSTEM_NAME=Android \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_SYSTEM_VERSION=28 \
  -DANDROID_PLATFORM=android-28 \
  -DANDROID_ABI=arm64-v8a \
  -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
  -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
  -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
  -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake \
  -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja \
  -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a \
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a \
  -DCMAKE_BUILD_TYPE=Debug \
  -B/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a \
  -GNinja

Starting process 'command '/usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake''. Working directory: /home/runner/work/PSX5/PSX5/app Command: /usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake -H/home/runner/work/PSX5/PSX5/app/src/main/cpp -DCMAKE_SYSTEM_NAME=Android -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_SYSTEM_VERSION=28 -DANDROID_PLATFORM=android-28 -DANDROID_ABI=arm64-v8a -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a -DCMAKE_BUILD_TYPE=Debug -B/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a -GNinja
Successfully started process 'command '/usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake''

> Task :app:compileDebugKotlin FAILED
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:361:52 Cannot infer type for this parameter. Please specify it explicitly.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:361:52 Not enough information to infer type argument for 'T'.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:361:52 Cannot infer type for this parameter. Please specify it explicitly.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:361:52 Not enough information to infer type argument for 'T'.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:361:61 Cannot infer type for this parameter. Please specify it explicitly.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:361:63 Cannot infer type for this parameter. Please specify it explicitly.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:361:63 Not enough information to infer type argument for 'T'.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:361:110 Unresolved reference 'getCrashLogs'.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:399:91 Unresolved reference 'getCrashLogs'.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:411:81 Unresolved reference 'clearCrashLogs'.
e: file:///home/runner/work/PSX5/PSX5/app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt:412:91 Unresolved reference 'getCrashLogs'.
Finished executing kotlin compiler using DAEMON strategy

> Task :app:configureCMakeDebug[arm64-v8a]
C/C++: CMake Warning at /home/runner/work/PSX5/PSX5/third_party/fex/External/range-v3/cmake/ranges_env.cmake:65 (message):
C/C++:   [range-v3 warning]: unknown system Android !
C/C++: Call Stack (most recent call first):
C/C++:   /home/runner/work/PSX5/PSX5/third_party/fex/External/range-v3/CMakeLists.txt:24 (include)
C/C++: CMake Warning at /home/runner/work/PSX5/PSX5/third_party/fex/External/range-v3/cmake/ranges_env.cmake:92 (message):
C/C++:   [range-v3 warning]: unknown build type, defaults to release!
C/C++: Call Stack (most recent call first):
C/C++:   /home/runner/work/PSX5/PSX5/third_party/fex/External/range-v3/CMakeLists.txt:24 (include)
C/C++: Traceback (most recent call last):
C/C++:   File "/home/runner/work/PSX5/PSX5/third_party/fex/Scripts/NeedDisabledSVE.py", line 73, in <module>
C/C++:     sys.exit(main())
C/C++:              ^^^^^^
C/C++:   File "/home/runner/work/PSX5/PSX5/third_party/fex/Scripts/NeedDisabledSVE.py", line 64, in main
C/C++:     if "sve" in Features:
C/C++:        ^^^^^^^^^^^^^^^^^
C/C++: TypeError: argument of type 'NoneType' is not iterable
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : Received process result: 0
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : Exiting generation of /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/compile_commands.json.bin normally
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : done executing cmake
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : hard linked /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/compile_commands.json to /home/runner/work/PSX5/PSX5/app/.cxx/tools/debug/arm64-v8a/compile_commands.json
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : JSON generation completed without problems
C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : Writing build model to /home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/logs/arm64-v8a/build_model.json
AAPT2 aapt2-8.7.3-12006047-linux Daemon #0: shutdown

FAILURE: Build failed with an exception.

* What went wrong:
Execution failed for task ':app:compileDebugKotlin'.
> A failure occurred while executing org.jetbrains.kotlin.compilerRunner.GradleCompilerRunnerWithWorkers$GradleKotlinCompilerWorkAction
   > Compilation error. See log for more details

* Try:
> Run with --debug option to get more log output.
> Run with --scan to get full insights.
> Get more help at https://help.gradle.org.

* Exception is:
org.gradle.api.tasks.TaskExecutionException: Execution failed for task ':app:compileDebugKotlin'.
	at org.gradle.api.internal.tasks.execution.ExecuteActionsTaskExecuter.lambda$executeIfValid$1(ExecuteActionsTaskExecuter.java:130)
	at org.gradle.internal.Try$Failure.ifSuccessfulOrElse(Try.java:293)
	at org.gradle.api.internal.tasks.execution.ExecuteActionsTaskExecuter.executeIfValid(ExecuteActionsTaskExecuter.java:128)
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
Caused by: org.gradle.workers.internal.DefaultWorkerExecutor$WorkExecutionException: A failure occurred while executing org.jetbrains.kotlin.compilerRunner.GradleCompilerRunnerWithWorkers$GradleKotlinCompilerWorkAction
	at org.gradle.workers.internal.DefaultWorkerExecutor$WorkItemExecution.waitForCompletion(DefaultWorkerExecutor.java:287)
	at org.gradle.internal.work.DefaultAsyncWorkTracker.lambda$waitForItemsAndGatherFailures$2(DefaultAsyncWorkTracker.java:130)
	at org.gradle.internal.Factories$1.create(Factories.java:31)
	at org.gradle.internal.work.DefaultWorkerLeaseService.withoutLocks(DefaultWorkerLeaseService.java:339)
	at org.gradle.internal.work.DefaultWorkerLeaseService.withoutLocks(DefaultWorkerLeaseService.java:322)
	at org.gradle.internal.work.DefaultWorkerLeaseService.withoutLock(DefaultWorkerLeaseService.java:327)
	at org.gradle.internal.work.DefaultAsyncWorkTracker.waitForItemsAndGatherFailures(DefaultAsyncWorkTracker.java:126)
	at org.gradle.internal.work.DefaultAsyncWorkTracker.waitForItemsAndGatherFailures(DefaultAsyncWorkTracker.java:92)
	at org.gradle.internal.work.DefaultAsyncWorkTracker.waitForAll(DefaultAsyncWorkTracker.java:78)
	at org.gradle.internal.work.DefaultAsyncWorkTracker.waitForCompletion(DefaultAsyncWorkTracker.java:66)
	at org.gradle.api.internal.tasks.execution.TaskExecution$3.run(TaskExecution.java:252)
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
	at org.gradle.internal.execution.steps.AbstractSkipEmptyWorkStep.execute(AbstractSkipEmptyWorkStep.java:61)
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
Caused by: org.jetbrains.kotlin.gradle.tasks.CompilationErrorException: Compilation error. See log for more details
	at org.jetbrains.kotlin.gradle.tasks.TasksUtilsKt.throwExceptionIfCompilationFailed(tasksUtils.kt:21)
	at org.jetbrains.kotlin.compilerRunner.GradleKotlinCompilerWork.run(GradleKotlinCompilerWork.kt:119)
	at org.jetbrains.kotlin.compilerRunner.GradleCompilerRunnerWithWorkers$GradleKotlinCompilerWorkAction.execute(GradleCompilerRunnerWithWorkers.kt:76)
	at org.gradle.workers.internal.DefaultWorkerServer.execute(DefaultWorkerServer.java:63)
	at org.gradle.workers.internal.NoIsolationWorkerFactory$1$1.create(NoIsolationWorkerFactory.java:66)
	at org.gradle.workers.internal.NoIsolationWorkerFactory$1$1.create(NoIsolationWorkerFactory.java:62)
	at org.gradle.internal.classloader.ClassLoaderUtils.executeInClassloader(ClassLoaderUtils.java:100)
	at org.gradle.workers.internal.NoIsolationWorkerFactory$1.lambda$execute$0(NoIsolationWorkerFactory.java:62)
	at org.gradle.workers.internal.AbstractWorker$1.call(AbstractWorker.java:44)
	at org.gradle.workers.internal.AbstractWorker$1.call(AbstractWorker.java:41)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:209)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$CallableBuildOperationWorker.execute(DefaultBuildOperationRunner.java:204)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:66)
	at org.gradle.internal.operations.DefaultBuildOperationRunner$2.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:166)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.execute(DefaultBuildOperationRunner.java:59)
	at org.gradle.internal.operations.DefaultBuildOperationRunner.call(DefaultBuildOperationRunner.java:53)
	at org.gradle.workers.internal.AbstractWorker.executeWrappedInBuildOperation(AbstractWorker.java:41)
	at org.gradle.workers.internal.NoIsolationWorkerFactory$1.execute(NoIsolationWorkerFactory.java:59)
	at org.gradle.workers.internal.DefaultWorkerExecutor.lambda$submitWork$0(DefaultWorkerExecutor.java:174)
	at org.gradle.internal.work.DefaultConditionalExecutionQueue$ExecutionRunner.runExecution(DefaultConditionalExecutionQueue.java:195)
	at org.gradle.internal.work.DefaultConditionalExecutionQueue$ExecutionRunner.access$700(DefaultConditionalExecutionQueue.java:128)
	at org.gradle.internal.work.DefaultConditionalExecutionQueue$ExecutionRunner$1.run(DefaultConditionalExecutionQueue.java:170)
	at org.gradle.internal.Factories$1.create(Factories.java:31)
	at org.gradle.internal.work.DefaultWorkerLeaseService.withLocks(DefaultWorkerLeaseService.java:267)
	at org.gradle.internal.work.DefaultWorkerLeaseService.runAsWorkerThread(DefaultWorkerLeaseService.java:131)
	at org.gradle.internal.work.DefaultWorkerLeaseService.runAsWorkerThread(DefaultWorkerLeaseService.java:136)
	at org.gradle.internal.work.DefaultConditionalExecutionQueue$ExecutionRunner.runBatch(DefaultConditionalExecutionQueue.java:165)
	at org.gradle.internal.work.DefaultConditionalExecutionQueue$ExecutionRunner.run(DefaultConditionalExecutionQueue.java:134)
	... 2 more


BUILD FAILED in 3m 48s
24 actionable tasks: 24 executed
```
