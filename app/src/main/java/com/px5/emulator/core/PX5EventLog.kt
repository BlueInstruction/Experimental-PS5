package com.px5.emulator.core

import android.content.Context
import android.util.Log
import java.io.File

/**
 * PX5EventLog — the app-wide EVENT/STATE/DIAGNOSTIC stream.
 *
 * Previously this lived as private methods on MainActivity, which meant
 * only onCreate could emit structured events. Driver import, driver
 * restore, dialogs and importers were invisible in px5_diagnostic.log —
 * a pasted log proved nothing about what the user had actually done.
 * This object makes the stream writable from anywhere, including
 * composables and IO coroutines.
 *
 * The NATIVE side (utils/diag_bridge.cpp) appends its filtered lines to
 * the same file with O_APPEND, so one pasted file now carries both the
 * Kotlin event story and the native loader/engine evidence.
 */
object PX5EventLog {

    private var logFile: File? = null
    private val tsFmt =
        java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", java.util.Locale.US)

    /** Call once in MainActivity.onCreate before any event emission. */
    fun init(context: Context) {
        try {
            val dir = context.getExternalFilesDir("logs")
                ?: File(context.filesDir, "logs")
            if (!dir.exists()) dir.mkdirs()
            // v1.16 LOG UNIFICATION: the EVENT/STATE stream lands in
            // px5_main.log itself — one pasted file now carries the session
            // banners, the Kotlin event story, the bridged native lines,
            // and the full crash reports inline. No more juggling
            // px5_diagnostic.log + px5_main.log to reconstruct a session.
            logFile = File(dir, "px5_main.log")
        } catch (_: Throwable) {
            logFile = null
        }
    }

    fun write(message: String) {
        try {
            Log.i("PX5", message)
            val f = logFile ?: return
            f.appendText("[${tsFmt.format(java.util.Date())}] $message\n")
        } catch (_: Throwable) {}
    }

    fun event(component: String, action: String,
              detail: String = "", result: String = "") {
        val msg = buildString {
            append("EVENT")
            append(" component=").append(component)
            append(" action=").append(action)
            if (detail.isNotEmpty()) append(" detail=").append(detail)
            if (result.isNotEmpty()) append(" result=").append(result)
        }
        write(msg)
    }

    fun state(component: String, state: String) {
        write("STATE component=$component state=$state")
    }

    fun exception(source: String, e: Throwable) {
        val sw = java.io.StringWriter()
        val pw = java.io.PrintWriter(sw)
        e.printStackTrace(pw)
        write("DIAGNOSTIC source=$source exception=${e.javaClass.name} " +
                "message=${e.message ?: "(none)"}\n${sw.toString().trim()}")
    }
}
