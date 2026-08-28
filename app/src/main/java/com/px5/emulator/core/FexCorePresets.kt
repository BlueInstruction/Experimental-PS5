package com.px5.emulator.core

import org.json.JSONObject

/**
 * FexCorePresets — pure CONFIGURATION for the FEXCore engine.
 *
 * Three separated concerns (configuration / runtime / diagnostics):
 *   * THIS file owns only preset data: named key->value maps over the REAL
 *     FEXCore option keys (FEXCore/Source/Interface/Config/Config.json.in —
 *     the same registry FEX's FEX_* environment layer writes through).
 *   * Application happens in the runtime path: MainActivity pushes every
 *     entry through FexCoreWrapper.nativeApplyEngineConfigOverride() BEFORE
 *     engine init, which lands in FEXCore::Config::Set() — the same public
 *     entry the official environment layer uses. No UI switch pretends to
 *     "enable" anything by itself.
 *   * Verification happens in diagnostics: Settings shows the engine's live
 *     counters (nativeGetEngineCounters) and the applied-override lines from
 *     the engine log, not a wishful toggle state.
 *
 * Values are strings exactly as FEXCore parses them ("0"/"1" booleans,
 * enum names for SMCChecks/HostFeatures, a number for MaxInst).
 */
object FexCorePresets {

    /** One option row for the editor UI. */
    data class Option(
        val key: String,
        val label: String,
        val values: List<String>,     // allowed values ("1"/"0" toggles included)
        val description: String
    )

    /** The 15 real FEXCore options this build exposes. Order = UI order. */
    val OPTIONS: List<Option> = listOf(
        Option("TSOEnabled", "TSO (total store order)",
            listOf("0", "1"),
            "Strong x86 memory ordering emulation. Safe default on; turning it off can speed up some games but breaks them more often."),
        Option("VectorTSOEnabled", "Vector TSO",
            listOf("0", "1"),
            "Applies TSO to vector (SSE/AVX) load-stores when TSO is on."),
        Option("HalfBarrierTSOEnabled", "Half-barrier TSO",
            listOf("0", "1"),
            "Unaligned loads/stores backpatched to half-barrier atomics when TSO is on."),
        Option("MemcpySetTSOEnabled", "Memcpy/memset TSO",
            listOf("0", "1"),
            "Applies TSO to memcpy/memset operations when TSO is on."),
        Option("X87ReducedPrecision", "X87 reduced precision",
            listOf("0", "1"),
            "Faster x87 float paths with slightly reduced precision."),
        Option("Multiblock", "Multiblock compilation",
            listOf("0", "1"),
            "Compile multiple blocks per pass. Usually faster, uses more memory."),
        Option("MaxInst", "Max instructions per block",
            listOf("5000"),
            "Upper bound of instructions compiled into one block."),
        Option("HostFeatures", "Host CPU feature policy",
            listOf("off", "enablesve", "disablesve", "enableavx", "disableavx"),
            "Force enable/disable specific host features. Keep 'off' unless debugging."),
        Option("SmallTSCScale", "Small TSC scaling",
            listOf("0", "1"),
            "Scale the guest timestamp counter up until it reaches the minimum frequency."),
        Option("SMCChecks", "SMC checks",
            listOf("none", "mtrack", "full"),
            "Self-modifying-code detection. PX5's engine default is mtrack (fault-tracked pages)."),
        Option("VolatileMetadata", "Volatile metadata (PE)",
            listOf("0", "1"),
            "Use PE volatile metadata to inform TSO ranges when available."),
        Option("MonoHacks", "Mono/.NET workarounds",
            listOf("0", "1"),
            "Workarounds for Mono runtime JIT patterns."),
        Option("HideHypervisorBit", "Hide hypervisor bit",
            listOf("0", "1"),
            "CPUID leaf 1 ECX bit 31 presentation to the guest."),
        Option("DisableL2Cache", "Disable L2 cache emulation",
            listOf("0", "1"),
            "Skip L2 cache emulation structures."),
        Option("DynamicL1Cache", "Dynamic L1 cache",
            listOf("0", "1"),
            "Grow the per-thread L1 lookup cache on demand.")
    )

    private val ALL_KEYS = OPTIONS.map { it.key }.toSet()

    /** A preset = display name + partial map of overrides (unset = engine default). */
    data class Preset(val name: String, val overrides: Map<String, String>)

    val BUILT_INS: List<Preset> = listOf(
        // Conservative correctness-first profile. TSO family fully on, SMC
        // mtrack (the engine default), multiblock off for the smallest JIT
        // surface. Slowest, least likely to miscompile.
        Preset("Safe (correctness)", mapOf(
            "TSOEnabled" to "1", "VectorTSOEnabled" to "1",
            "HalfBarrierTSOEnabled" to "1", "MemcpySetTSOEnabled" to "1",
            "Multiblock" to "0", "SMCChecks" to "mtrack",
            "HostFeatures" to "off"
        )),
        // Default-ish: engine defaults plus multiblock for throughput.
        Preset("Balanced", mapOf(
            "TSOEnabled" to "1", "HalfBarrierTSOEnabled" to "1",
            "Multiblock" to "1", "SMCChecks" to "mtrack",
            "HostFeatures" to "off"
        )),
        // Throughput profile for strong devices (handhelds with active
        // cooling): TSO still on (games break without it), vector/memcpy
        // TSO off where the guest tolerates it, multiblock on.
        Preset("Performance", mapOf(
            "TSOEnabled" to "1", "VectorTSOEnabled" to "0",
            "MemcpySetTSOEnabled" to "0", "Multiblock" to "1",
            "SmallTSCScale" to "1", "SMCChecks" to "mtrack",
            "HostFeatures" to "off"
        )),
        // Debug profile: full SMC checks and no speculative JIT features.
        Preset("Debug / bring-up", mapOf(
            "Multiblock" to "0", "SMCChecks" to "full",
            "HostFeatures" to "off", "HideHypervisorBit" to "1"
        ))
    )

    /** Filtered copy: keeps only known keys and allowed values. Honest validation. */
    fun sanitize(overrides: Map<String, String>): Map<String, String> =
        overrides.filterKeys { it in ALL_KEYS }
            .filter { (k, v) ->
                OPTIONS.find { it.key == k }?.values?.contains(v) == true
            }

    /** Serialize the active custom override map into Px5Settings storage. */
    fun encode(overrides: Map<String, String>): String =
        JSONObject(sanitize(overrides)).toString()

    /** Decode stored JSON back to a validated override map. */
    fun decode(json: String): Map<String, String> = runCatching {
        val o = JSONObject(json)
        val m = mutableMapOf<String, String>()
        for (key in o.keys()) m[key] = o.optString(key, "")
        sanitize(m)
    }.getOrDefault(emptyMap())
}
