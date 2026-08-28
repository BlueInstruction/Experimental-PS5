package com.px5.emulator.core;

import android.view.Surface;

/**
 * JNI surface v2 (honest contract).
 *
 * Every method maps to a REAL native action. Nothing here returns a
 * fabricated success: failures come back as false / explanatory strings.
 * See cpp/fexcore_wrapper.cpp for the native side of each call.
 */
public class FexCoreWrapper {

    static {
        System.loadLibrary("px5");
    }

    public native String stringFromJNI();

    // FEXCore runtime (kept behind UI buttons; known to crash on some
    // devices inside InitCore — tracked in issue notes)
    public native boolean initializeFexCore();
    public native void nativeShutdown();

    // Real file copy install (std::filesystem backed)
    public native boolean nativeInstallPkg(String pkgPath, String destPath);

    // Real ELF64 parse + map into guest window; SELF is honestly rejected
    // until Phase C implements decryption.
    public native boolean nativeLoadElf(String elfPath);
    public native boolean nativeLoadSelf(String selfPath);

    /**
     * Fork-isolated JIT conformance (mov/add/hlt -> RAX=42).
     * Returns the REAL report string: "PASSED — ...", "FAILED — ...", or
     * "... CRASHED in isolated child (signal N)" when the JIT faulted. A
     * fault kills the test child, never the app; the crash handler writes a
     * full register dump to px5_crash_<timestamp>.log either way.
     */
    public native String nativeRunCpuConformanceTest();

    /**
     * One-time runtime wiring (call in Application/MainActivity onCreate):
     *  - installs the native crash handler writing px5_crash.log
     *  - hands driver-loader directories to GpuDriverManager (adrenotools)
     */
    public native void nativeInitRuntimeContext(String logsDir, String hookLibDir,
                                                String tmpLibDir, String driverRootDir);

    // Memory window tests (validated against canonical window now)
    public native long nativeMapMemory(long addr, long size, int flags);
    public native boolean nativeUnmapMemory(long addr, long size);

    // Architecture status strings
    public native String nativeGetArchitectureSummary();

    /** Live engine counters (syscalls, SMC, memory window, thread state). */
    public native String nativeGetEngineCounters();

    /**
     * Applies one FEXCore config override through the real layered config
     * (FEXCore::Config::Set). Keys mirror FEX's own FEX_* options:
     * TSOEnabled, VectorTSOEnabled, HalfBarrierTSOEnabled, MemcpySetTSOEnabled,
     * X87ReducedPrecision, Multiblock, MaxInst, HostFeatures, SmallTSCScale,
     * SMCChecks, VolatileMetadata, MonoHacks, HideHypervisorBit,
     * DisableL2Cache, DynamicL1Cache. Must run BEFORE engine init; returns
     * false (logged) when the key is unknown or the engine is already live.
     */
    public native boolean nativeApplyEngineConfigOverride(String key, String value);

    /** Kotlin ids: 0=none 1=error 2=warn 3=info 4=debug 5=trace. */
    public native void nativeSetLogLevel(int level);

    /** Kotlin ids: 0=auto 1=FIFO 2=FIFO_RELAXED 3=MAILBOX 4=IMMEDIATE
     *  5=FIFO_LATEST_READY. Validated against the device at swapchain build. */
    public native void nativeSetPresentMode(int mode);

    // ===== Foundation evidence pipeline (NEW, replaces fake layers) =====

    /**
     * Runs the ordered honest self-test:
     *   1. memory window reservation      4. raw x86-64 write+exit(42)+halt
     *   2. Vulkan runtime enumeration     5. real ELF loader round-trip
     *   3. FEXCore context creation
     * Returns a multi-line report ending with "VERDICT: PASS|FAIL".
     */
    public native String nativeRunFoundationSelfTest();

    /** Real dlopen'd Vulkan summary ("Vulkan: ACTIVE | Adreno ... " or error). */
    public native String nativeGetVulkanSummary();

    // ===== Phase 2 surface (real GPU proof / renderer / settings / input) ==

    /**
     * Logical-device + offscreen clear submission proof.
     * Returns "PASS | detail" or "FAIL | detail" — never a silent success.
     */
    public native String nativeRunGpuProof();

    /** Attach an android.view.Surface; creates the Vulkan swapchain lazily. */
    public native boolean nativeAttachRenderSurface(Surface surface);

    public native void   nativeDetachRenderSurface();
    public native boolean nativeStartRenderer();
    public native void   nativeStopRenderer();

    /** Live render loop stats string ("GPU device | frames | present mode"). */
    public native String nativeGetRenderStats();

    /**
     * Pushes UI settings into the engine atomics. Real effects:
     * resScalePct clamps to [50..200]; vsync picks MAILBOX/IMMEDIATE vs
     * FIFO at swapchain build; verboseLog flips the C++ logger level;
     * logDir enables on-disk rotating logs (idempotent, first call wins).
     */
    public native void nativeApplySettings(int resScalePct, boolean vsync,
                                           int driverModeSlot,
                                           boolean verboseLog, String logDir);

    /** libkernel HLE symbol-table summary (counts real invocations). */
    public native String nativeGetKernelHleSummary();

    // ---- Input bridge ------------------------------------------------------
    /** One of PadButtons bit constants from NativeInput class below. */
    public native boolean nativeSetButtonState(int buttonBit, boolean pressed);
    /** Analog left stick, normalized [-1..1]. Real atomics, real readback. */
    public native boolean nativeSetLeftStick(float lx, float ly);
    /** Analog right stick, normalized [-1..1]. */
    public native boolean nativeSetRightStick(float rx, float ry);
    /** Analog triggers L2/R2, normalized [0..1]. */
    public native boolean nativeSetTriggers(float l2, float r2);
    /** Clickable touchpad button state. */
    public native boolean nativeSetTouchpad(boolean pressed);
    public native String  nativeGetInputSummary();

    // ---- Driver slots -------------------------------------------------------
    /**
     * Registers a user-imported driver directory (already extracted &
     * structurally validated by Kotlin). Returns slot id >=1, 0 on reject.
     */
    public native int     nativeRegisterDriverSlot(String label, String soPath, String soname);
    /** Mode 0 = system ICD; >0 selects a registered slot for next init. */
    public native boolean nativeSetDriverMode(int mode);
    /** Drops every registered slot (falls back to system ICD until re-registered). */
    public native void    nativeClearDriverSlots();
    public native String  nativeGetDriverManagerSummary();

    /** Shared PadButtons bit definitions (mirrors cpp input/controller.h). */
    public static final int PAD_CROSS      = 1 << 0;
    public static final int PAD_CIRCLE     = 1 << 1;
    public static final int PAD_SQUARE     = 1 << 2;
    public static final int PAD_TRIANGLE   = 1 << 3;
    public static final int PAD_DPAD_UP    = 1 << 4;
    public static final int PAD_DPAD_DOWN  = 1 << 5;
    public static final int PAD_DPAD_LEFT  = 1 << 6;
    public static final int PAD_DPAD_RIGHT = 1 << 7;
    public static final int PAD_L1         = 1 << 8;
    public static final int PAD_R1         = 1 << 9;
    public static final int PAD_OPTIONS    = 1 << 10;
    public static final int PAD_SHARE      = 1 << 11;
    public static final int PAD_PS_HOME    = 1 << 12;
}
