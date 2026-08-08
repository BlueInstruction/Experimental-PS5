package com.px5.emulator.core;

public class FexCoreWrapper {

    static {
        System.loadLibrary("px5");
    }

    public native String stringFromJNI();

    public native boolean initializeFexCore();

    public native void nativeShutdown();

    public native boolean nativeInstallPkg(String pkgPath, String destPath);

    public native boolean nativeLoadElf(String elfPath);

    public native boolean nativeLoadSelf(String selfPath);

    public native String nativeLoadElfPackage(String elfPath);

    public native boolean nativeRun();

    public native void nativePause();

    public native void nativeResume();

    public native boolean nativeStep();

    public native boolean nativeRunCpuConformanceTest();

    public native void nativeReset();

    public native long nativeMapMemory(long addr, long size, int flags);

    public native boolean nativeUnmapMemory(long addr, long size);

    public native String nativeGetArchitectureSummary();

    public native boolean nativeLoadThunksConfig(String thunksJsonStr);

    public native boolean nativeLoadFexConfig(String fexConfigJsonStr);

    // libadrenotools & Turnip Vulkan Driver Hook Methods
    public native boolean nativeInitAdrenotools(String driverDir, String libName, String hookLib);

    public native String nativeGetTurnipDriverInfo();

    public native void nativeSetTurnipBcnTextureSupport(boolean enabled);

    public native void nativeSetTurnipPipelineCaching(boolean enabled);
}
