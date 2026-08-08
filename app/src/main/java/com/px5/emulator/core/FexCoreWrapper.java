package com.px5.emulator.core;

public class FexCoreWrapper {

    static {
        System.loadLibrary("px5");
    }

    public native String stringFromJNI();

    public native boolean initializeFexCore();

    public native boolean nativeInstallPkg(String pkgPath, String destPath);

    public native String nativeLoadElfPackage(String elfPath);

    public native String nativeGetArchitectureSummary();

    // libadrenotools & Turnip Vulkan Driver Hook Methods
    public native boolean nativeInitAdrenotools(String driverDir, String libName, String hookLib);

    public native String nativeGetTurnipDriverInfo();

    public native void nativeSetTurnipBcnTextureSupport(boolean enabled);

    public native void nativeSetTurnipPipelineCaching(boolean enabled);
}
