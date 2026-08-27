package com.px5.emulator.core;

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

    // Legacy arithmetic JIT conformance (mov/add/hlt -> RAX=42)
    public native boolean nativeRunCpuConformanceTest();

    // Memory window tests (validated against canonical window now)
    public native long nativeMapMemory(long addr, long size, int flags);
    public native boolean nativeUnmapMemory(long addr, long size);

    // Architecture status strings
    public native String nativeGetArchitectureSummary();

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
}
