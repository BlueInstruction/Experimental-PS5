# Build Summary

- gradle exit code: 1

## Matched failure patterns
```
325:Resource missing. [HTTP GET: https://dl.google.com/dl/android/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.pom]
327:Downloading https://repo.maven.apache.org/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.pom to /home/runner/.gradle/.tmp/gradle_download8416794124432202072bin
481:Downloading https://repo.maven.apache.org/maven2/com/google/guava/failureaccess/1.0.1/failureaccess-1.0.1.jar to /home/runner/.gradle/.tmp/gradle_download14965187481833140112bin
632:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with InstrumentationAnalysisTransform
1050:Caching disabled for InstrumentationAnalysisTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
1363:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with InstrumentationAnalysisTransform
1364:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with MergeInstrumentationAnalysisTransform
3524:Caching disabled for MergeInstrumentationAnalysisTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
3527:Transforming failureaccess-1.0.1.jar (com.google.guava:failureaccess:1.0.1) with ExternalDependencyInstrumentingArtifactTransform
3531:Caching disabled for ExternalDependencyInstrumentingArtifactTransform: /home/runner/.gradle/caches/modules-2/files-2.1/com.google.guava/failureaccess/1.0.1/1dcf1de382a0bf95a3d8b0849546c88bac1292c9/failureaccess-1.0.1.jar because:
9074:C/C++: TypeError: argument of type 'NoneType' is not iterable
9209:C/C++: /usr/bin/ld.gold: error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: could not load plugin library: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: cannot open shared object file: No such file or directory
9210:C/C++: /usr/bin/ld.gold: fatal error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/28/crtbegin_so.o: unsupported ELF machine number 183
9211:C/C++: clang++: error: linker command failed with exit code 1 (use -v to see invocation)
9217:> Task :app:buildCMakeDebug[arm64-v8a] FAILED
9219:FAILURE: Build failed with an exception.
9442:  /usr/bin/ld.gold: error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: could not load plugin library: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: cannot open shared object file: No such file or directory
9443:  /usr/bin/ld.gold: fatal error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/28/crtbegin_so.o: unsupported ELF machine number 183
9444:  clang++: error: linker command failed with exit code 1 (use -v to see invocation)
9473:	at org.gradle.internal.Try$Failure.ifSuccessfulOrElse(Try.java:293)
9501:	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
9503:Caused by: org.gradle.internal.UncheckedException: com.android.ide.common.process.ProcessException: ninja: Entering directory `/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
9722:/usr/bin/ld.gold: error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: could not load plugin library: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: cannot open shared object file: No such file or directory
9723:/usr/bin/ld.gold: fatal error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/28/crtbegin_so.o: unsupported ELF machine number 183
9724:clang++: error: linker command failed with exit code 1 (use -v to see invocation)
9861:	at org.gradle.internal.concurrent.ExecutorPolicy$CatchAndRecordFailures.onExecute(ExecutorPolicy.java:64)
9863:Caused by: com.android.ide.common.process.ProcessException: ninja: Entering directory `/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
10082:/usr/bin/ld.gold: error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: could not load plugin library: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: cannot open shared object file: No such file or directory
10083:/usr/bin/ld.gold: fatal error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/28/crtbegin_so.o: unsupported ELF machine number 183
10084:clang++: error: linker command failed with exit code 1 (use -v to see invocation)
10121:Caused by: com.android.ide.common.process.ProcessException: Error while executing process /usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja with arguments {-C /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a CodeSizeValidation FEX FEXBash FEXCore_shared FEXGetConfig FEXOfflineCompiler FEXRootFSFetcher FEXServer FEXpidof px5}
10126:Caused by: org.gradle.process.internal.ExecException: Process 'command '/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja'' finished with non-zero exit value 1
10132:BUILD FAILED in 7m 11s
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
3770:Tasks to be executed: [task ':app:preBuild', task ':app:preDebugBuild', task ':app:mergeDebugNativeDebugMetadata', task ':app:checkKotlinGradlePluginConfigurationErrors', task ':app:checkDebugAarMetadata', task ':app:generateDebugResValues', task ':app:mapDebugSourceSetPaths', task ':app:generateDebugResources', task ':app:mergeDebugResources', task ':app:packageDebugResources', task ':app:parseDebugLocalResources', task ':app:createDebugCompatibleScreenManifests', task ':app:extractDeepLinksDebug', task ':app:processDebugMainManifest', task ':app:processDebugManifest', task ':app:processDebugManifestForPackage', task ':app:processDebugResources', task ':app:kspDebugKotlin', task ':app:compileDebugKotlin', task ':app:javaPreCompileDebug', task ':app:compileDebugJavaWithJavac', task ':app:mergeDebugShaders', task ':app:compileDebugShaders', task ':app:generateDebugAssets', task ':app:mergeDebugAssets', task ':app:compressDebugAssets', task ':app:desugarDebugFileDependencies', task ':app:dexBuilderDebug', task ':app:mergeDebugGlobalSynthetics', task ':app:processDebugJavaRes', task ':app:mergeDebugJavaResource', task ':app:checkDebugDuplicateClasses', task ':app:mergeExtDexDebug', task ':app:mergeLibDexDebug', task ':app:mergeProjectDexDebug', task ':app:configureCMakeDebug[arm64-v8a]', task ':app:buildCMakeDebug[arm64-v8a]', task ':app:configureCMakeDebug[armeabi-v7a]', task ':app:buildCMakeDebug[armeabi-v7a]', task ':app:configureCMakeDebug[x86]', task ':app:buildCMakeDebug[x86]', task ':app:configureCMakeDebug[x86_64]', task ':app:buildCMakeDebug[x86_64]', task ':app:mergeDebugJniLibFolders', task ':app:mergeDebugNativeLibs', task ':app:stripDebugDebugSymbols', task ':app:validateSigningDebug', task ':app:writeDebugAppMetadata', task ':app:writeDebugSigningConfigVersions', task ':app:packageDebug', task ':app:createDebugApkListingFileRedirect', task ':app:assembleDebug']
8817:INFO: D8: Some warnings are typically a sign of using an outdated Java toolchain. To fix, recompile the source with an updated toolchain.
8860:Resolve mutations for :app:configureCMakeDebug[arm64-v8a] (Thread[Execution worker,5,main]) started.
8861::app:configureCMakeDebug[arm64-v8a] (Thread[Execution worker,5,main]) started.
8978:> Task :app:configureCMakeDebug[arm64-v8a]
8979:Caching disabled for task ':app:configureCMakeDebug[arm64-v8a]' because:
8982:Task ':app:configureCMakeDebug[arm64-v8a]' is not up-to-date because:
8990:C/C++: Parsing /usr/local/lib/android/sdk/cmake/3.22.1/package.xml
8991:C/C++: Parsing /usr/local/lib/android/sdk/cmake/3.31.5/package.xml
8992:C/C++: Parsing /usr/local/lib/android/sdk/cmake/4.1.2/package.xml
8998:C/C++: Parsing /usr/local/lib/android/sdk/ndk/27.3.13750724/package.xml
8999:C/C++: Parsing /usr/local/lib/android/sdk/ndk/28.2.13676358/package.xml
9000:C/C++: Parsing /usr/local/lib/android/sdk/ndk/29.0.14206865/package.xml
9016:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : Start JSON generation. Platform version: 28 min SDK version: arm64-v8a
9017:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : rebuilding JSON /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a/android_gradle_build.json due to:
9018:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : - no fingerprint file, will remove stale configuration folder
9019:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : removing stale contents from '/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
9020:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : created folder '/home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a'
9021:C/C++: /home/runner/work/PSX5/PSX5/app/src/main/cpp/CMakeLists.txt debug|arm64-v8a : executing cmake /usr/local/lib/android/sdk/cmake/3.22.1/bin/cmake \
9023:  -DCMAKE_SYSTEM_NAME=Android \
9024:  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
9025:  -DCMAKE_SYSTEM_VERSION=28 \
9028:  -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
9029:  -DANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
9030:  -DCMAKE_ANDROID_NDK=/usr/local/lib/android/sdk/ndk/27.3.13750724 \
9031:  -DCMAKE_TOOLCHAIN_FILE=/usr/local/lib/android/sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake \
9032:  -DCMAKE_MAKE_PROGRAM=/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja \
9033:  -DCMAKE_LIBRARY_OUTPUT_DIRECTORY=/home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a \
(no matches)
```

## Last 400 lines
```
      FEX \
      FEXBash \
      FEXCore_shared \
      FEXGetConfig \
      FEXOfflineCompiler \
      FEXRootFSFetcher \
      FEXServer \
      FEXpidof \
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
[1/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_add.c.o
[2/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_eq_signaling.c.o
[3/293] Building C object fex/External/xxhash/cmake_unofficial/CMakeFiles/xxhash.dir/home/runner/work/PSX5/PSX5/third_party/fex/External/xxhash/xxhash.c.o
[4/293] Linking C static library fex/External/xxhash/cmake_unofficial/libxxhash.a
[5/293] Building C object fex/External/tiny-json/CMakeFiles/tiny-json.dir/tiny-json.c.o
[6/293] Linking C static library fex/External/tiny-json/libtiny-json.a
[7/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_div.c.o
[8/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_sub.c.o
[9/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_mul.c.o
[10/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_rem.c.o
[11/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_sqrt.c.o
[12/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_le.c.o
[13/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_i32.c.o
[14/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_i64.c.o
[15/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_ui64.c.o
[16/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_f32.c.o
[17/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_f64.c.o
[18/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/i32_to_extF80.c.o
[19/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/ui64_to_extF80.c.o
[20/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_to_f128.c.o
[21/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_extF80.c.o
[22/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_add.c.o
[23/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_div.c.o
[24/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_isSignalingNaN.c.o
[25/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_eq.c.o
[26/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_le_quiet.c.o
[27/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_lt_quiet.c.o
[28/293] Building CXX object fex/External/fmt/CMakeFiles/fmt.dir/src/os.cc.o
[29/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_le.c.o
[30/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normRoundPackToF128.c.o
[31/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_lt.c.o
[32/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_mulAdd.c.o
[33/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_mul.c.o
[34/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_rem.c.o
[35/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_sqrt.c.o
[36/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_sub.c.o
[37/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_f16.c.o
[38/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_f32.c.o
[39/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_f64.c.o
[40/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_i32.c.o
[41/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_i64.c.o
[42/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_ui32.c.o
[43/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f128_to_ui64.c.o
[44/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_addMagsF128.c.o
[45/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_subMagsF128.c.o
[46/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_propagateNaNF128UI.c.o
[47/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_f128UIToCommonNaN.c.o
[48/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundPackToF128.c.o
[49/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/i32_to_f128.c.o
[50/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f32_to_f128.c.o
[51/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundToUI64.c.o
[52/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalF128Sig.c.o
[53/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_commonNaNToF128UI.c.o
[54/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_extF80UIToCommonNaN.c.o
[55/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundToI32.c.o
[56/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundToI64.c.o
[57/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundPackToF32.c.o
[58/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_addMagsExtF80.c.o
[59/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_commonNaNToF32UI.c.o
[60/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_propagateNaNExtF80UI.c.o
[61/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundPackToF64.c.o
[62/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_commonNaNToF64UI.c.o
[63/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_roundPackToExtF80.c.o
[64/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_shiftRightJam128.c.o
[65/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_subMagsExtF80.c.o
[66/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalExtF80Sig.c.o
[67/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_shiftRightJam128Extra.c.o
[68/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normRoundPackToExtF80.c.o
[69/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_approxRecip_1Ks.c.o
[70/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_approxRecipSqrt32_1.c.o
[71/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_approxRecipSqrt_1Ks.c.o
[72/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/softfloat_raiseFlags.c.o
[73/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_commonNaNToExtF80UI.c.o
[74/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f64_to_extF80.c.o
[75/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalF64Sig.c.o
[76/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_f64UIToCommonNaN.c.o
[77/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_roundToInt.c.o
[78/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_eq.c.o
[79/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/extF80_lt.c.o
[80/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_normSubnormalF32Sig.c.o
[81/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/f32_to_extF80.c.o
[82/293] Building C object fex/External/SoftFloat-3e/CMakeFiles/softfloat_3e.dir/src/s_f32UIToCommonNaN.c.o
[83/293] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/polevll.c.o
[84/293] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/log2ll.c.o
[85/293] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/mtherr.c.o
[86/293] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/exp2ll.c.o
[87/293] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/floorll.c.o
[88/293] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/constll.c.o
[89/293] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/atanll.c.o
[90/293] Linking C static library fex/External/SoftFloat-3e/libsoftfloat_3e.a
[91/293] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/sinll.c.o
[92/293] Building C object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/tanll.c.o
[93/293] Building CXX object fex/External/cephes/CMakeFiles/cephes_128bit.dir/src/128bit/Impl.cpp.o
[94/293] Linking CXX static library fex/External/cephes/libcephes_128bit.a
[95/293] Building CXX object fex/FEXCore/Source/CMakeFiles/JemallocLibs.dir/Utils/AllocatorHooks.cpp.o
[96/293] Linking CXX static library fex/FEXCore/Source/libJemallocLibs.a
[97/293] Building CXX object fex/External/fmt/CMakeFiles/fmt.dir/src/format.cc.o
[98/293] Generating ../../../IR.md
[99/293] Linking CXX static library fex/External/fmt/libfmtd.a
[100/293] Generating ../../../include/FEXCore/Config/ConfigValues.inl, ../../../include/FEXCore/Config/ConfigOptions.inl, ../../../generated/FEX.1
[101/293] Generating ../../../generated/FEX.1.gz
[102/293] Building CXX object fex/FEXCore/Source/CMakeFiles/JemallocDummy.dir/Utils/AllocatorHooks.cpp.o
[103/293] Generating ../../../include/FEXCore/IR/IRDefines.inc, ../../../include/FEXCore/IR/IRDefines_Dispatch.inc
[104/293] Linking CXX static library fex/FEXCore/Source/libJemallocDummy.a
[105/293] Building CXX object CMakeFiles/px5.dir/fexcore_wrapper.cpp.o
[106/293] Building CXX object CMakeFiles/px5.dir/core/emulator.cpp.o
[107/293] Building CXX object CMakeFiles/px5.dir/fexcore_integration.cpp.o
[108/293] Building CXX object CMakeFiles/px5.dir/kernel/signals.cpp.o
[109/293] Building CXX object CMakeFiles/px5.dir/memory/memory.cpp.o
[110/293] Building CXX object CMakeFiles/px5.dir/memory/memory_map.cpp.o
[111/293] Building CXX object CMakeFiles/px5.dir/audio/audio.cpp.o
[112/293] Building CXX object CMakeFiles/px5.dir/kernel/syscalls.cpp.o
[113/293] Building CXX object CMakeFiles/px5.dir/kernel_hle.cpp.o
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
[114/293] Building CXX object CMakeFiles/px5.dir/loader/self_loader.cpp.o
[115/293] Building CXX object CMakeFiles/px5.dir/loader/elf_loader.cpp.o
[116/293] Building CXX object CMakeFiles/px5.dir/gpu/vulkan_device.cpp.o
[117/293] Building CXX object CMakeFiles/px5.dir/gpu/shader_cache.cpp.o
[118/293] Building CXX object CMakeFiles/px5.dir/gnm_vulkan_renderer.cpp.o
[119/293] Building CXX object CMakeFiles/px5.dir/filesystem/vfs.cpp.o
[120/293] Building CXX object CMakeFiles/px5.dir/audio_input_native.cpp.o
[121/293] Building CXX object CMakeFiles/px5.dir/input/controller.cpp.o
[122/293] Building CXX object CMakeFiles/px5.dir/utils/logger.cpp.o
[123/293] Building CXX object CMakeFiles/px5.dir/turnip_hook.cpp.o
[124/293] Building CXX object CMakeFiles/px5.dir/utils/linker_ns_bypass.cpp.o
[125/293] Building CXX object CMakeFiles/px5.dir/media/media_codec_hle.cpp.o
[126/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Utils/FileLoading.cpp.o
[127/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Utils/SpinWaitLock.cpp.o
[128/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Utils/ForcedAssert.cpp.o
[129/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Utils/Allocator.cpp.o
[130/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Interface/Config/Config.cpp.o
[131/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Utils/WildcardMatcher.cpp.o
[132/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Utils/Allocator/64BitAllocator.cpp.o
[133/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_Base.dir/Utils/LogManager.cpp.o
[134/293] Linking CXX static library fex/FEXCore/Source/libFEXCore_Base.a
[135/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Common/JitSymbols.cpp.o
[136/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/LookupCache.cpp.o
[137/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Context/Context.cpp.o
[138/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/CPUBackend.cpp.o
[139/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Addressing.cpp.o
[140/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/CodeCache.cpp.o
[141/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Core.cpp.o
[142/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/SharedCodeBufferManager.cpp.o
[143/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/CPUID.cpp.o
[144/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Frontend.cpp.o
[145/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/Crypto.cpp.o
[146/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/AVX_128.cpp.o
[147/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/Flags.cpp.o
[148/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/Vector.cpp.o
[149/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/X87.cpp.o
[150/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/X87F64.cpp.o
[151/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher.cpp.o
[152/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/ArchHelpers/Arm64Emitter.cpp.o
[153/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Dispatcher/Dispatcher.cpp.o
[154/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Interpreter/Fallbacks/InterpreterFallbacks.cpp.o
[155/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Interpreter/Fallbacks/StringCompareFallbacks.cpp.o
[156/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/ALUOps.cpp.o
[157/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/AtomicOps.cpp.o
[158/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/JIT.cpp.o
[159/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/BranchOps.cpp.o
[160/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/ConversionOps.cpp.o
[161/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/EncryptionOps.cpp.o
[162/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/MiscOps.cpp.o
[163/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/MoveOps.cpp.o
[164/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/MemoryOps.cpp.o
[165/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/Arm64Relocations.cpp.o
[166/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/VectorOps.cpp.o
[167/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/BaseTables.cpp.o
[168/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/DDDTables.cpp.o
[169/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/H0F38Tables.cpp.o
[170/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/H0F3ATables.cpp.o
[171/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/PrimaryGroupTables.cpp.o
[172/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/SecondaryModRMTables.cpp.o
[173/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/SecondaryGroupTables.cpp.o
[174/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/VEXTables.cpp.o
[175/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/SecondaryTables.cpp.o
[176/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/X87Tables.cpp.o
[177/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/GDBJIT/GDBJIT.cpp.o
[178/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/IRDumper.cpp.o
[179/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/IRDumperPass.cpp.o
[180/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/IREmitter.cpp.o
[181/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/PassManager.cpp.o
[182/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/IRValidation.cpp.o
[183/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/RedundantFlagCalculationElimination.cpp.o
[184/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/RegisterAllocationPass.cpp.o
[185/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/LongJump.cpp.o
[186/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/ArchHelpers/Arm64.cpp.o
[187/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/Threads.cpp.o
[188/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/x87StackOptimizationPass.cpp.o
[189/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/Telemetry.cpp.o
[190/293] Building CXX object fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/Profiler.cpp.o
[191/293] Linking CXX static library fex/FEXCore/Source/libFEXCore.a
[192/293] Building CXX object fex/Source/Common/CMakeFiles/Common.dir/CPUInfo.cpp.o
[193/293] Linking CXX shared library /home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a/libFEXCore.so
FAILED: /home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a/libFEXCore.so 
: && /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ --target=aarch64-none-linux-android28 --sysroot=/usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/sysroot -fPIC -g -DANDROID -fdata-sections -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -Wformat -Werror=format-security   -fno-limit-debug-info  -flto=thin  -static-libstdc++ -Wl,--build-id=sha1 -Wl,--no-rosegment -Wl,--no-undefined-version -Wl,--fatal-warnings -Wl,--no-undefined -Qunused-arguments    -fuse-ld=gold -shared -Wl,-soname,libFEXCore.so -o /home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a/libFEXCore.so fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Common/JitSymbols.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Context/Context.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/LookupCache.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/CodeCache.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Core.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/CPUBackend.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Addressing.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/CPUID.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Frontend.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/SharedCodeBufferManager.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/AVX_128.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/Crypto.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/Flags.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/Vector.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/X87.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher/X87F64.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/OpcodeDispatcher.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/ArchHelpers/Arm64Emitter.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Dispatcher/Dispatcher.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Interpreter/Fallbacks/InterpreterFallbacks.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/Interpreter/Fallbacks/StringCompareFallbacks.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/JIT.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/ALUOps.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/AtomicOps.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/BranchOps.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/ConversionOps.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/EncryptionOps.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/MemoryOps.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/MiscOps.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/MoveOps.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/VectorOps.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/JIT/Arm64Relocations.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/BaseTables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/DDDTables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/H0F38Tables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/H0F3ATables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/PrimaryGroupTables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/SecondaryGroupTables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/SecondaryModRMTables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/SecondaryTables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/VEXTables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/Core/X86Tables/X87Tables.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/GDBJIT/GDBJIT.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/IRDumper.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/IREmitter.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/PassManager.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/IRDumperPass.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/IRValidation.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/RedundantFlagCalculationElimination.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/RegisterAllocationPass.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Interface/IR/Passes/x87StackOptimizationPass.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/LongJump.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/Telemetry.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/Threads.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/Profiler.cpp.o fex/FEXCore/Source/CMakeFiles/FEXCore_object.dir/Utils/ArchHelpers/Arm64.cpp.o  fex/FEXCore/Source/libJemallocLibs.a  -latomic -lm && :
/usr/bin/ld.gold: error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: could not load plugin library: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/bin/../lib/LLVMgold.so: cannot open shared object file: No such file or directory
/usr/bin/ld.gold: fatal error: /usr/local/lib/android/sdk/ndk/27.3.13750724/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/28/crtbegin_so.o: unsupported ELF machine number 183
clang++: error: linker command failed with exit code 1 (use -v to see invocation)
[194/293] Building CXX object fex/Source/Common/CMakeFiles/Common.dir/Config.cpp.o
[195/293] Linking CXX shared library /home/runner/work/PSX5/PSX5/app/build/intermediates/cxx/Debug/67652l3u/obj/arm64-v8a/libpx5.so
ninja: build stopped: subcommand failed.

C++ build system [build] failed while executing:
    /usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja \
      -C \
      /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a \
      CodeSizeValidation \
      FEX \
      FEXBash \
      FEXCore_shared \
      FEXGetConfig \
      FEXOfflineCompiler \
      FEXRootFSFetcher \
      FEXServer \
      FEXpidof \
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
Caused by: com.android.ide.common.process.ProcessException: Error while executing process /usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja with arguments {-C /home/runner/work/PSX5/PSX5/app/.cxx/Debug/67652l3u/arm64-v8a CodeSizeValidation FEX FEXBash FEXCore_shared FEXGetConfig FEXOfflineCompiler FEXRootFSFetcher FEXServer FEXpidof px5}
	at com.android.build.gradle.internal.process.GradleProcessResult.buildProcessException(GradleProcessResult.java:73)
	at com.android.build.gradle.internal.process.GradleProcessResult.assertNormalExitValue(GradleProcessResult.java:48)
	at com.android.build.gradle.internal.cxx.process.ExecuteProcessKt.execute(ExecuteProcess.kt:277)
	... 131 more
Caused by: org.gradle.process.internal.ExecException: Process 'command '/usr/local/lib/android/sdk/cmake/3.22.1/bin/ninja'' finished with non-zero exit value 1
	at org.gradle.process.internal.DefaultExecHandle$ExecResultImpl.assertNormalExitValue(DefaultExecHandle.java:442)
	at com.android.build.gradle.internal.process.GradleProcessResult.assertNormalExitValue(GradleProcessResult.java:46)
	... 132 more


BUILD FAILED in 7m 11s
25 actionable tasks: 25 executed
```
