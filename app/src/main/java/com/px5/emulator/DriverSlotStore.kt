package com.px5.emulator

import android.content.Context
import com.px5.emulator.core.FexCoreWrapper
import org.json.JSONArray
import java.io.File

/**
 * DriverSlotStore — persistence for imported GPU driver slots.
 *
 * The native GpuDriverManager keeps slots in a process-lifetime vector;
 * without this store every cold start forgot imported Turnip packages
 * while Px5Settings.driverMode still pointed at a slot that no longer
 * existed. Now: slots are saved as JSON in app prefs, re-registered into
 * the native manager on every startup, and the saved active mode is
 * re-applied only when it still points at a real slot.
 */
object DriverSlotStore {

    data class Slot(val label: String, val soPath: String,
                    val soname: String = "libvulkan_adreno.so")

    private const val PREFS = "px5_engine_settings"
    private const val KEY_SLOTS = "driverSlots"

    fun load(context: Context): List<Slot> = runCatching {
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val arr = JSONArray(prefs.getString(KEY_SLOTS, "[]") ?: "[]")
        (0 until arr.length()).mapNotNull { i ->
            val o = arr.optJSONObject(i) ?: return@mapNotNull null
            val label = o.optString("label", "")
            val soPath = o.optString("soPath", "")
            // Older saves predate meta.json support: they always shipped the
            // normalized soname, so that is the correct default here.
            val soname = o.optString("soname", "libvulkan_adreno.so")
                .ifBlank { "libvulkan_adreno.so" }
            if (label.isNotBlank() && soPath.isNotBlank())
                Slot(label, soPath, soname) else null
        }
    }.getOrDefault(emptyList())

    fun save(context: Context, slots: List<Slot>) {
        val arr = JSONArray()
        slots.forEach { s ->
            arr.put(org.json.JSONObject()
                .put("label", s.label)
                .put("soPath", s.soPath)
                .put("soname", s.soname))
        }
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit().putString(KEY_SLOTS, arr.toString()).apply()
    }

    /**
     * Re-register every persisted slot whose .so still exists into the
     * native manager, in the saved order (so slot ids match the saved
     * active mode). Returns the number of live slots.
     *
     * onSlot fires per restored slot so callers can put the restore story
     * into the shared event stream (label/soname/path) — without it, a
     * pasted diagnostic log only ever said "restored_N" with no detail.
     */
    fun restore(context: Context, wrapper: FexCoreWrapper?,
                onSlot: ((label: String, soPath: String, soname: String) -> Unit)? = null): Int {
        if (wrapper == null) return 0
        val live = load(context).filter { File(it.soPath).isFile }
        // Drop entries pointing at files that vanished (cleared cache etc.)
        if (live.size != load(context).size) save(context, live)
        // v1.23: the native manager OUTLIVES the activity. The 2026-08-31
        // 02:57-03:15 device session showed pid 22745 constant across five
        // activity re-creations — every restore() used to APPEND a duplicate
        // slot to the surviving vector (slot ids marched 1->2->3->4->5->6 in
        // the log). Clear the native list first so re-registration starts
        // from a clean state and ids match the saved order again.
        runCatching { wrapper.nativeClearDriverSlots() }
        live.forEachIndexed { index, slot ->
            runCatching {
                wrapper.nativeRegisterDriverSlot(slot.label, slot.soPath, slot.soname)
            }
                .onSuccess { id ->
                    onSlot?.invoke(slot.label, slot.soPath, slot.soname)
                    if (id.toInt() != index + 1) {
                        val msg = "driver slot id drift: expected ${index + 1}, got $id"
                        android.util.Log.w("PX5", msg)
                        com.px5.emulator.core.PX5EventLog.event(
                            "drivers", "slot_id_drift",
                            "expected=${index + 1} got=$id label=${slot.label}")
                    }
                }
                .onFailure { f ->
                    com.px5.emulator.core.PX5EventLog.exception(
                        "DriverSlotStore.restore(${slot.label})", f)
                }
        }
        return live.size
    }

    fun append(context: Context, slot: Slot) {
        val slots = load(context).toMutableList()
        slots += slot
        save(context, slots)
    }

    fun remove(context: Context, index: Int) {
        val slots = load(context).toMutableList()
        if (index in slots.indices) {
            slots.removeAt(index)
            save(context, slots)
        }
    }
}
