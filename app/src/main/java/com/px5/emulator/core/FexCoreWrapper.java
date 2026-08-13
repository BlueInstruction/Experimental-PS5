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

    public native boolean nativeRunCpuConformanceTest();

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

    // ---- Logging system (added by fix/logging-system) ----

    /**
     * Initialize the native file logger + native crash handler.
     * Call this once at app startup (PX5Application.onCreate).
     *
     * @param logDir Absolute path to a writable directory where log files
     *               will be created. Typically
     *               {@code /storage/emulated/0/Android/data/com.px5.emulator/files/logs}.
     * @return true if both the logger and crash handler initialized OK.
     */
    public native boolean nativeInitLogger(String logDir);

    /** Returns the absolute path of the current main log file (px5_main.log). */
    public native String nativeGetLogFilePath();

    /** Flushes pending log writes to disk (useful before sharing logs). */
    public native void nativeFlushLogs();

    /**
     * Sets the minimum log level for the native logger.
     * @param level 0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=FATAL.
     */
    public native void nativeSetLogLevel(int level);
}
