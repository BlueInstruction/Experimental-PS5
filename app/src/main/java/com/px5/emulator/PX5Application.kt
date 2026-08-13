package com.px5.emulator

import android.app.Application
import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * PX5Application — simple, reliable crash logging.
 *
 * This is a deliberate return to the old PX5-style logging approach, which
 * was simpler and more useful than the over-engineered native crash handler
 * + file logger + logcat capture combo that replaced it.
 *
 * What the old system did right:
 *   - Caught Kotlin/Java exceptions via Thread.setDefaultUncaughtExceptionHandler
 *   - Wrote the FULL stack trace to a single file (px5_crash_log.txt)
 *   - Also kept a copy in SharedPreferences as a fallback
 *   - Had a simple 50KB cap to avoid filling storage
 *
 * What the new system did wrong:
 *   - The native crash handler (sigaction + SIGSEGV) produced useless
 *     backtraces on stripped binaries (just "pc 0x..." with no symbol names)
 *   - The logcat capture thread was broken (px5_logcat.log was always empty)
 *   - The file logger (px5_main.log) only recorded "Logger initialized"
 *     messages because no other code was wired to use PX5_LOGI etc.
 *   - The per-crash file naming (px5_crash_<ts>_<pid>.log) spammed the
 *     log directory in a crash loop
 *
 * This version keeps the old simple approach and adds:
 *   - A single fixed filename (px5_crash_log.txt) — overwritten on each
 *     crash so we never spam files
 *   - The native crash handler from utils/crash_handler.cpp is still
 *     installed as a safety net for actual native SIGSEGVs, but the
 *     Kotlin handler is the primary logging path
 */
class PX5Application : Application() {

    override fun onCreate() {
        super.onCreate()
        instance = this

        // Resolve the log directory early so both handlers can use it.
        val logsDir = File(getExternalFilesDir(null), "logs").apply {
            if (!exists()) mkdirs()
        }

        // Install the native crash handler as a safety net for SIGSEGV/SIGABRT.
        // It writes to px5_crash.log (single file, overwritten).
        // This only fires on actual native crashes — Kotlin exceptions go
        // through the handler below.
        try {
            val wrapper = com.px5.emulator.core.FexCoreWrapper()
            val ok = wrapper.nativeInitLogger(logsDir.absolutePath)
            Log.i(TAG, "Native logger init: $ok (dir=${logsDir.absolutePath})")
        } catch (e: UnsatisfiedLinkError) {
            Log.e(TAG, "Failed to load libpx5.so; native crash handler disabled", e)
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error initializing native logger", e)
        }

        // Install the Kotlin-side uncaught exception handler.
        // This is the PRIMARY crash logging path — it catches all Java/Kotlin
        // exceptions (including Compose UI crashes like the font loading bug)
        // and writes the full stack trace to px5_crash_log.txt.
        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            try {
                val sw = StringWriter()
                val pw = PrintWriter(sw)
                throwable.printStackTrace(pw)
                val stackTrace = sw.toString()

                val timeStamp = SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date())
                val logEntry = "[$timeStamp] Thread: ${thread.name}\n$stackTrace\n"

                saveCrashLog(logEntry)
                Log.e("PX5CrashHandler", "Uncaught Exception Captured:\n$logEntry")
            } catch (e: Exception) {
                Log.e("PX5CrashHandler", "Failed to save crash log", e)
            } finally {
                defaultHandler?.uncaughtException(thread, throwable)
            }
        }

        Log.i(TAG, "PX5Application.onCreate done — crash logging initialized")
    }

    private fun saveCrashLog(log: String) {
        val prefs = getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
        val existingLogs = prefs.getString(KEY_CRASH_LOGS, "") ?: ""
        val newLogs = "$log\n--- LOG ENTRY END ---\n\n$existingLogs"
        // Keep logs within reasonable size limit (50KB max)
        val trimmedLogs = if (newLogs.length > 50000) newLogs.substring(0, 50000) else newLogs
        prefs.edit().putString(KEY_CRASH_LOGS, trimmedLogs).apply()

        // Also write to Android/data/com.px5.emulator/files/logs/px5_crash_log.txt
        try {
            val logsDir = getExternalFilesDir("logs")
            if (logsDir != null) {
                if (!logsDir.exists()) logsDir.mkdirs()
                val logFile = File(logsDir, "px5_crash_log.txt")
                logFile.writeText(trimmedLogs)
            }
        } catch (e: Exception) {
            Log.e("PX5CrashHandler", "Failed writing log file to external data dir", e)
        }
    }

    companion object {
        private const val TAG = "PX5Application"
        private const val PREF_NAME = "px5_debug_prefs"
        private const val KEY_CRASH_LOGS = "crash_logs"

        lateinit var instance: PX5Application
            private set

        fun getCrashLogs(context: Context): String {
            val prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
            val logs = prefs.getString(KEY_CRASH_LOGS, "") ?: ""
            return if (logs.isBlank()) "No crash or error logs recorded." else logs
        }

        fun clearCrashLogs(context: Context) {
            val prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
            prefs.edit().remove(KEY_CRASH_LOGS).apply()
            try {
                val logsDir = context.getExternalFilesDir("logs")
                if (logsDir != null) {
                    val logFile = File(logsDir, "px5_crash_log.txt")
                    if (logFile.exists()) logFile.delete()
                }
            } catch (e: Exception) {
                Log.e("PX5CrashHandler", "Failed deleting external log file", e)
            }
        }

        fun logSystemEvent(context: Context, tag: String, message: String) {
            val timeStamp = SimpleDateFormat("HH:mm:ss", Locale.US).format(Date())
            val logEntry = "[$timeStamp] [$tag] $message\n"
            val prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
            val existingLogs = prefs.getString(KEY_CRASH_LOGS, "") ?: ""
            val newLogs = "$logEntry$existingLogs"
            val trimmedLogs = if (newLogs.length > 50000) newLogs.substring(0, 50000) else newLogs
            prefs.edit().putString(KEY_CRASH_LOGS, trimmedLogs).apply()

            try {
                val logsDir = context.getExternalFilesDir("logs")
                if (logsDir != null) {
                    if (!logsDir.exists()) logsDir.mkdirs()
                    val logFile = File(logsDir, "px5_crash_log.txt")
                    logFile.writeText(trimmedLogs)
                }
            } catch (e: Exception) {
                Log.e("PX5CrashHandler", "Failed writing log file to external data dir", e)
            }
        }
    }
}
