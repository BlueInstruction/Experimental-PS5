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

    // Real ELF64 parse + map into guest window; SELF containers go through
    // the real extractor — unencrypted/fake-signed dumps load their inner
    // ELF, encrypted segments are counted and refused by name (no keys,
    // no pretending).
    public native boolean nativeLoadElf(String elfPath);
    public native boolean nativeLoadSelf(String selfPath);

    /**
     * Format-agnostic load for the game-boot flow: the native side peeks
     * the file's own magic (SELF 0x1D22154F vs ELF 0x7F 'E' 'L' 'F') and
     * dispatches to the matching loader. Returns the real result; the
     * reason for any rejection is in logcat + the breadcrumb ring.
     */
    public native boolean nativeLoadExecutable(String path);

    /**
     * v1.12 crash containment: runs the ENTIRE load pipeline (file read,
     * SELF extraction, ELF parse, guest-window mapping) inside a
     * fork-isolated child. A load-stage native fault costs the probe
     * child, never the app — the returned report names the real signal
     * plus the VERIFIED crash-dump path (the parent stats the dump file
     * before claiming it exists). Call this BEFORE
     * {@link #nativeLoadExecutable(String)}: only a report starting with
     * "LOAD OK" should promote to the real in-process mapping.
     */
    public native String nativeLoadExecutableIsolated(String path);

    /**
     * v1.13 execution containment: runs the FULL game pipeline — load
     * (SELF extract / ELF parse / guest-window map) AND real guest
     * execution at the image entry — inside a fork-isolated child with a
     * hard timeout (timeoutMs; 0 waits forever). A fault then costs the
     * probe child plus a VERIFIED register dump, and a hang costs the
     * timeout budget — never the app process. Report contract:
     * "EXEC EXIT n (…)", "EXEC RETURNED without clean exit (…)",
     * "LOAD FAILED: <reason>", "execution probe: TIMEOUT after n ms (…)",
     * or "execution probe: CRASHED in isolated child (signal N)" followed
     * by the verified dump line. Headless by design: nothing renders.
     */
    public native String nativeRunExecutionProbe(String path, int timeoutMs);

    /**
     * Fork-isolated JIT conformance (mov/add/hlt -> RAX=42).
     * Returns the REAL report string: "PASSED — ...", "FAILED — ...", or
     * "... CRASHED in isolated child (signal N)" when the JIT faulted. A
     * fault kills the test child, never the app. When the child's crash
     * handler managed to write the register dump, the report names the
     * VERIFIED dump path ("register dump verified: <path> (<n> bytes)");
     * when it could not, the report says exactly that — no unverified
     * promises either way.
     */
    public native String nativeRunCpuConformanceTest();

    /**
     * v1.20 — the in-process JIT conformance. SAME synthetic blob, NO fork.
     *
     * The 2026-08-31 device session proved the fork child dies inside
     * ExecuteThread (SIGSEGV si_addr=0x4) even after a full engine rebuild,
     * with the identical signature since v1.15. This call answers the one
     * remaining question: does the FEXCore JIT work on this device at all?
     *
     * HONEST CONTRACT — READ BEFORE CALLING: without the fork there is no
     * containment. If the JIT faults, the APP PROCESS dies. The armed
     * crash handler writes the full evidence-first report (module-resolved
     * PC, faulting instruction bytes, backtrace) into px5_main.log before
     * the process dies, and the user relaunches. The UI must label this
     * button as unsafe. Returns "PASSED — ..." or "FAILED — ..." only when
     * the process survived.
     */
    public native String nativeRunCpuConformanceInProcess();

    /**
     * One-time runtime wiring (call in Application/MainActivity onCreate):
     *  - installs the native crash handler writing px5_crash.log
     *  - hands driver-loader directories to GpuDriverManager (adrenotools)
     *  - stamps the build identity into the NATIVE log streams too, so any
     *    pasted slice of any log file answers "which APK produced this?"
     */
    public native void nativeInitRuntimeContext(String logsDir, String hookLibDir,
                                                String tmpLibDir, String driverRootDir,
                                                String buildIdentity);

    /**
     * Kotlin->native event passthrough for boot-critical moments. Writes a
     * INFO line into px5_main.log and the bridged diagnostic stream, so the
     * engine-log side of a paste shows the game-boot path even if the
     * process dies before anything else logs. category "gameBoot" routes to
     * the LOADER log category; anything else goes to CORE.
     */
    public native void nativeLogEvent(String category, String message);

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

    /**
     * Phase C milestone 1: synthetic-stream GNM PM4 decoder self-test
     * (pure CPU). Returns the decoder's own multi-line report starting
     * with "PASS" or "FAIL" plus per-subtest results. Proves decoder +
     * state-model mechanics only — not game compatibility.
     */
    public native String nativeRunGnmSelfTest();

    /**
     * Phase C milestone 2a: SELF container extractor self-test (synthetic
     * SELF images: plain segment, zlib-compressed segment, encrypted
     * segment refused BY NAME, bad magic refused). Proves loader
     * mechanics only — real game SELF files are parsed in the loader
     * milestone, and encrypted segments are never decrypted (no keys,
     * no pretending).
     */
    public native String nativeRunLoaderSelfTest();

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
     * vramUsageMode (0=conservative 1=balanced 2=aggressive) biases image
     * memory-type selection in VulkanGpuDevice; logDir enables on-disk
     * rotating logs (idempotent, first call wins).
     */
    public native void nativeApplySettings(int resScalePct, boolean vsync,
                                           int driverModeSlot,
                                           boolean verboseLog,
                                           int vramUsageMode, String logDir);

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

    /**
     * v1.16 eager driver verification: dlopens the active slot's driver
     * (no Vulkan instance, no GPU context) and re-runs the maps proof, so
     * the manager summary answers driverVerified=yes/NO immediately after
     * import or selection instead of staying "not-run". Returns the fresh
     * manager summary string.
     */
    public native String  nativeVerifyDriverSlot(int slotIndex);

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
    /** V2 pad: stick-click buttons. The native InputManager stores the full
     *  32-bit mask; guest-side routing for these two lands with kernel HLE. */
    public static final int PAD_L3         = 1 << 13;
    public static final int PAD_R3         = 1 << 14;
}
