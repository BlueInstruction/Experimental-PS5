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
    }

    /** Applies the persisted orientation mode to the host activity. */
    fun applyOrientation(activity: android.app.Activity) {
        when (_orientationMode.value) {
            1 -> activity.requestedOrientation =
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
            2 -> activity.requestedOrientation =
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_USER_PORTRAIT
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
            dir.ifEmpty { null }
        )
    }
}
