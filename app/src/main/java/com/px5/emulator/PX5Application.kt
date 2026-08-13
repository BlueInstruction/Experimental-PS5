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
 * PX5Application — exact copy of the old PX5 repo's logging approach.
 *
 * - Thread.setDefaultUncaughtExceptionHandler catches Kotlin/Java exceptions
 * - Writes the full stack trace to:
 *     /storage/emulated/0/Android/data/com.px5.emulator/files/logs/px5_crash_log.txt
 * - Also keeps a copy in SharedPreferences (px5_debug_prefs / crash_logs)
 * - 50KB cap
 *
 * The native crash handler + file logger + logcat capture from the
 * fix/logging-system branch have been REMOVED — they were producing
 * useless output (stripped binary backtraces, empty logcat file, etc).
 *
 * IMPORTANT: libpx5.so is NOT loaded here. It is loaded lazily by
 * FexCoreWrapper's static initializer when MainActivity first references
 * it. This ensures that if the native lib fails to load, the
 * UncaughtExceptionHandler is already installed and can capture the
 * UnsatisfiedLinkError stack trace.
 */
class PX5Application : Application() {

    override fun onCreate() {
        super.onCreate()
        instance = this

        // Pre-create the logs directory so saveCrashLog() never fails on
        // a missing directory.
        try {
            val logsDir = getExternalFilesDir("logs")
            if (logsDir != null && !logsDir.exists()) {
                logsDir.mkdirs()
            }
        } catch (e: Exception) {
            Log.e("PX5CrashHandler", "Failed to create logs dir", e)
        }

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
