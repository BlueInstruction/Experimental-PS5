package com.px5.emulator.core

import android.content.Context
import android.content.SharedPreferences
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow

/**
 * Px5Settings — single source of truth for engine-facing user preferences.
 *
 * Values persist in SharedPreferences AND are pushed into the native layer
 * through FexCoreWrapper.nativeApplySettings(), where they genuinely affect
 * swapchain present mode / extent handling and the C++ log level.
 */
object Px5Settings {

    private const val PREFS = "px5_engine_settings"

    private val _resScalePct = MutableStateFlow(100)
    val resScalePct: StateFlow<Int> = _resScalePct

    private val _vsyncEnabled = MutableStateFlow(true)
    val vsyncEnabled: StateFlow<Boolean> = _vsyncEnabled

    private val _verboseLogging = MutableStateFlow(false)
    val verboseLogging: StateFlow<Boolean> = _verboseLogging

    private val _driverMode = MutableStateFlow(0)     // 0 = system ICD
    val driverMode: StateFlow<Int> = _driverMode

    // ---- Appearance / shell (UI-only, not pushed to native) ---------------
    /** 0 = Dark (default), 1 = Light, 2 = Follow system. */
    private val _themeMode = MutableStateFlow(0)
    val themeMode: StateFlow<Int> = _themeMode

    /** 0 = follow system, 1 = force landscape, 2 = force portrait. */
    private val _orientationMode = MutableStateFlow(0)
    val orientationMode: StateFlow<Int> = _orientationMode

    /** Show the on-screen DualSense pad on the emulation stage. */
    private val _showTouchPad = MutableStateFlow(true)
    val showTouchPad: StateFlow<Boolean> = _showTouchPad

    /** Opacity of the on-screen pad overlay (40..100 %). UI-only. */
    private val _touchOpacityPct = MutableStateFlow(100)
    val touchOpacityPct: StateFlow<Int> = _touchOpacityPct

    /** Global size of the on-screen pad (60..160 %). UI-only. */
    private val _padScalePct = MutableStateFlow(100)
    val padScalePct: StateFlow<Int> = _padScalePct

    /** Persisted V2 pad layout: JSON map element-name -> [fx, fy]
     *  (fractional center coordinates on the stage). Empty = defaults. */
    private val _padLayoutJson = MutableStateFlow("")
    val padLayoutJson: StateFlow<String> = _padLayoutJson

    /** Overlay: show live FPS (computed from the real frame counter). */
    private val _showFps = MutableStateFlow(false)
    val showFps: StateFlow<Boolean> = _showFps

    /** Overlay: show live frame time in milliseconds. */
    private val _showFrametime = MutableStateFlow(false)
    val showFrametime: StateFlow<Boolean> = _showFrametime

    /** VRAM usage mode: 0=conservative 1=balanced 2=aggressive.
     *  Pushed to native and consumed by VulkanGpuDevice memory-type
     *  selection (see settings.h / vulkan_device.cpp). */
    private val _vramUsageMode = MutableStateFlow(1)
    val vramUsageMode: StateFlow<Int> = _vramUsageMode

    /** Explicit engine log level: 0=none 1=error 2=warn 3=info 4=debug 5=trace,
     *  -1 = never chosen (legacy verbose toggle decides). */
    private val _logLevel = MutableStateFlow(-1)
    val logLevel: StateFlow<Int> = _logLevel

    /** Swapchain present mode: 0=auto 1=FIFO 2=FIFO_RELAXED 3=MAILBOX
     *  4=IMMEDIATE 5=FIFO_LATEST_READY (device-validated at swapchain build). */
    private val _presentMode = MutableStateFlow(0)
    val presentMode: StateFlow<Int> = _presentMode

    /** Active FEXCore preset name + sanitized custom overrides (JSON map). */
    private val _enginePresetName = MutableStateFlow("Balanced")
    val enginePresetName: StateFlow<String> = _enginePresetName
    private val _engineOverrides = MutableStateFlow<Map<String, String>>(emptyMap())
    val engineOverrides: StateFlow<Map<String, String>> = _engineOverrides

    private var prefs: SharedPreferences? = null

    fun init(context: Context) {
        if (prefs != null) return
        prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        _resScalePct.value   = prefs?.getInt("resScalePct", 100) ?: 100
        _vsyncEnabled.value  = prefs?.getBoolean("vsync", true) ?: true
        _verboseLogging.value= prefs?.getBoolean("verbose", false) ?: false
        _driverMode.value    = prefs?.getInt("driverMode", 0) ?: 0
        _themeMode.value     = prefs?.getInt("themeMode", 0) ?: 0
        _orientationMode.value = prefs?.getInt("orientationMode", 0) ?: 0
        _showTouchPad.value  = prefs?.getBoolean("showTouchPad", true) ?: true
        _touchOpacityPct.value = prefs?.getInt("touchOpacityPct", 100) ?: 100
        _padScalePct.value = prefs?.getInt("padScalePct", 100) ?: 100
        _padLayoutJson.value = prefs?.getString("padLayoutV2", "") ?: ""
        _showFps.value       = prefs?.getBoolean("showFps", false) ?: false
        _showFrametime.value = prefs?.getBoolean("showFrametime", false) ?: false
        _vramUsageMode.value = prefs?.getInt("vramUsageMode", 1) ?: 1
        _logLevel.value      = prefs?.getInt("logLevel", -1) ?: -1
        _presentMode.value   = prefs?.getInt("presentMode", 0) ?: 0
        _enginePresetName.value = prefs?.getString("enginePresetName", "Balanced")
            ?: "Balanced"
        _engineOverrides.value =
            FexCorePresets.decode(prefs?.getString("engineOverrides", "") ?: "")
    }

    /** Applies the persisted orientation mode to the host activity.
     *  Mode 0 now explicitly releases any previous override (the emulation
     *  stage forces landscape while open and hands the shell back here). */
    fun applyOrientation(activity: android.app.Activity) {
        when (_orientationMode.value) {
            1 -> activity.requestedOrientation =
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
            2 -> activity.requestedOrientation =
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
            else -> activity.requestedOrientation =
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
        }
    }

    fun setResScalePct(v: Int) {
        val clamped = v.coerceIn(50, 200)
        _resScalePct.value = clamped
        prefs?.edit()?.putInt("resScalePct", clamped)?.apply()
        push()
    }

    fun setVsync(on: Boolean) {
        _vsyncEnabled.value = on
        prefs?.edit()?.putBoolean("vsync", on)?.apply()
        push()
    }

    fun setVerbose(on: Boolean) {
        _verboseLogging.value = on
        prefs?.edit()?.putBoolean("verbose", on)?.apply()
        push()
    }

    fun setDriverMode(mode: Int) {
        _driverMode.value = mode.coerceAtLeast(0)
        prefs?.edit()?.putInt("driverMode", _driverMode.value)?.apply()
        push()
    }

    fun setThemeMode(mode: Int) {
        _themeMode.value = mode.coerceIn(0, 2)
        prefs?.edit()?.putInt("themeMode", _themeMode.value)?.apply()
    }

    fun setOrientationMode(mode: Int) {
        _orientationMode.value = mode.coerceIn(0, 2)
        prefs?.edit()?.putInt("orientationMode", _orientationMode.value)?.apply()
    }

    fun setShowTouchPad(on: Boolean) {
        _showTouchPad.value = on
        prefs?.edit()?.putBoolean("showTouchPad", on)?.apply()
    }

    fun setTouchOpacityPct(v: Int) {
        _touchOpacityPct.value = v.coerceIn(40, 100)
        prefs?.edit()?.putInt("touchOpacityPct", _touchOpacityPct.value)?.apply()
    }

    fun setPadScalePct(v: Int) {
        _padScalePct.value = v.coerceIn(60, 160)
        prefs?.edit()?.putInt("padScalePct", _padScalePct.value)?.apply()
    }

    fun setPadLayoutJson(json: String) {
        _padLayoutJson.value = json
        prefs?.edit()?.putString("padLayoutV2", json)?.apply()
    }

    fun setShowFps(on: Boolean) {
        _showFps.value = on
        prefs?.edit()?.putBoolean("showFps", on)?.apply()
    }

    fun setShowFrametime(on: Boolean) {
        _showFrametime.value = on
        prefs?.edit()?.putBoolean("showFrametime", on)?.apply()
    }

    fun setVramUsageMode(mode: Int) {
        val v = mode.coerceIn(0, 2)
        _vramUsageMode.value = v
        prefs?.edit()?.putInt("vramUsageMode", v)?.apply()
        push()
    }

    fun setLogLevel(level: Int) {
        val v = level.coerceIn(-1, 5)
        _logLevel.value = v
        prefs?.edit()?.putInt("logLevel", v)?.apply()
    }

    fun setPresentMode(mode: Int) {
        val v = mode.coerceIn(0, 5)
        _presentMode.value = v
        prefs?.edit()?.putInt("presentMode", v)?.apply()
    }

    /** Stores a preset selection and returns the effective override map. */
    fun setEnginePreset(name: String, overrides: Map<String, String>): Map<String, String> {
        val clean = FexCorePresets.sanitize(overrides)
        _enginePresetName.value = name
        _engineOverrides.value = clean
        prefs?.edit()
            ?.putString("enginePresetName", name)
            ?.putString("engineOverrides", FexCorePresets.encode(clean))
            ?.apply()
        return clean
    }

    /**
     * Push current values into the native engine. Safe before the wrapper
     * exists (no-op) and safe to call repeatedly.
     */
    fun push(wrapper: FexCoreWrapper? = null, context: Context? = null) {
        val dir = try {
            context?.getExternalFilesDir("logs")?.absolutePath ?: ""
        } catch (_: Throwable) { "" }
        val w = wrapper ?: return       // native can't receive without lib
        w.nativeApplySettings(
            resScalePct.value,
            vsyncEnabled.value,
            driverMode.value,
            verboseLogging.value,
            vramUsageMode.value,
            dir.ifEmpty { null }
        )
    }
}
