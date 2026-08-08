package com.px5.emulator

import android.app.Application
import android.content.Context
import android.content.SharedPreferences
import android.util.Log
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class PX5Application : Application() {

    override fun onCreate() {
        super.onCreate()
        instance = this
        
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
                val logFile = java.io.File(logsDir, "px5_crash_log.txt")
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
                    val logFile = java.io.File(logsDir, "px5_crash_log.txt")
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
                    val logFile = java.io.File(logsDir, "px5_crash_log.txt")
                    logFile.writeText(trimmedLogs)
                }
            } catch (e: Exception) {
                Log.e("PX5CrashHandler", "Failed writing log file to external data dir", e)
            }
        }
    }
}
