package com.px5.emulator

import android.app.Application
import android.content.Context
import android.util.Log
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * PX5Application — crash logger that catches ALL uncaught exceptions,
 * including those from the Compose UI render thread.
 *
 * The previous version only wrote a "diagnostic" log that showed
 * MainActivity.onCreate steps but MISSED the actual crash because:
 *   1. Compose UI crashes happen on a different thread (main thread's
 *      Choreographer frame callback, NOT the thread that called onCreate)
 *   2. The crash happened AFTER writeDiagnosticLog() returned, so the
 *      diagnostic log only showed "FexCoreWrapper() constructed" and
 *      nothing after
 *
 * This version installs a global UncaughtExceptionHandler that:
 *   - Catches exceptions on ANY thread (main, render, background)
 *   - Writes the FULL stack trace to px5_crash_<timestamp>.log
 *   - Also mirrors to Android logcat (so adb logcat shows it)
 *   - Calls the default handler afterward so the OS still gets the crash
 *
 * The crash log file is written to:
 *   /storage/emulated/0/Android/data/com.px5.emulator/files/logs/
 *   px5_crash_<yyyy-MM-dd_HH-mm-ss>.log
 *
 * Each crash gets its own timestamped file (not overwritten) so we can
 * see the full crash history across multiple launch attempts.
 */
class PX5Application : Application() {

    override fun onCreate() {
        super.onCreate()
        instance = this

        // Install the global crash handler FIRST, before anything else.
        // This ensures that even if SoundManager init or libpx5.so load
        // throws, we capture it.
        installCrashHandler()

        // v1.14 post-mortem: a SIGKILL-class death (lmkd / ANR watchdog /
        // system force-stop) cannot run ANY in-process handler, so it
        // leaves NO px5_crash_*.log — exactly the "app dies on game boot
        // with zero logs" report. The OS still records the death in the
        // logcat buffer, and an app may read its OWN buffer on the next
        // launch. Capture the death markers into a file the Crashes list
        // already surfaces (px5_crash_*_postmortem.log).
        Thread {
            try {
                val dir = getLogDirectory(this)
                dir.mkdirs()
                val proc = Runtime.getRuntime()
                    .exec(arrayOf("logcat", "-d", "-t", "2000"))
                val out = proc.inputStream.bufferedReader().readText()
                proc.waitFor()
                // v1.16: "process_crashed" added — a HANDLED native crash
                // (our crash handler reports and re-raises) never produces
                // the OS's "Fatal signal" line, which is why every v1.15
                // session answered markers=0 while real crashes were
                // happening. Our own FATAL evidence is now counted.
                val patterns = listOf(
                    "Fatal signal", "FATAL EXCEPTION", "has died",
                    "ANR in com.px5.emulator", "Force finishing",
                    "lowmemorykiller", "Low Memory Killer", "tombstoned",
                    "process_crashed", "PX5: nested fault inside crash handler")
                val hits = out.lineSequence().filter { line ->
                    patterns.any { line.contains(it, ignoreCase = true) } ||
                        line.contains("com.px5.emulator")
                }.toList()
                if (hits.isNotEmpty()) {
                    val ts = SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US)
                        .format(Date())
                    val f = File(dir, "px5_crash_${ts}_postmortem.log")
                    f.writeText(buildString {
                        appendLine("==== PX5 POST-MORTEM: previous session death markers ====")
                        appendLine("captured: $ts — source: logcat -d (own-UID entries only)")
                        appendLine("A SIGKILL-class kill cannot run any in-process handler;")
                        appendLine("these lines are the OS's own records of what happened.")
                        appendLine("Pair with px5_heartbeat.log (1 Hz last live breadcrumb).")
                        appendLine()
                        hits.forEach { appendLine(it) }
                    })
                    // v1.16 UNIFIED LOG: a compact section lands in
                    // px5_main.log too — the single file the user pastes
                    // now also names the previous session's death.
                    try {
                        File(dir, "px5_main.log").appendText(buildString {
                            appendLine()
                            appendLine("==== PX5 POSTMORTEM @ $ts ====")
                            appendLine("previous-session death markers (${hits.size}) — " +
                                "full list: ${f.name}")
                            hits.take(30).forEach { appendLine("  $it") }
                            if (hits.size > 30) appendLine("  ... (${hits.size - 30} more)")
                            appendLine("==== PX5 POSTMORTEM END ====")
                            appendLine()
                        })
                    } catch (_: Throwable) {}
                    com.px5.emulator.core.PX5EventLog.event("diag", "postmortem",
                        "markers=${hits.size} file=${f.name}")
                } else {
                    com.px5.emulator.core.PX5EventLog.event("diag", "postmortem",
                        "markers=0 (no death records in the logcat buffer)")
                }
            } catch (t: Throwable) {
                Log.w("PX5", "post-mortem capture failed: ${t.message}")
            }
        }.start()

        Log.i(TAG, "PX5Application.onCreate — crash handler installed")
    }

    private fun installCrashHandler() {
        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()

        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            try {
                // 1. Write to logcat FIRST (this always works, even if
                //    file write fails)
                Log.e(TAG, "=== UNCAUGHT EXCEPTION on thread '${thread.name}' ===", throwable)

                // 2. Write to crash file
                saveCrashLog(thread, throwable)
            } catch (e: Exception) {
                // If our own crash handler crashes, at least try to log it
                Log.e(TAG, "Crash handler itself failed: ${e.message}", e)
            } finally {
                // 3. Call the default handler so the OS shows the "app
                //    stopped" dialog and writes a tombstone
                defaultHandler?.uncaughtException(thread, throwable)
            }
        }
    }

    private fun saveCrashLog(thread: Thread, throwable: Throwable) {
        try {
            // This resolves to:
            // /storage/emulated/0/Android/data/com.px5.emulator/files/logs/
            val logDir = File(getExternalFilesDir(null), "logs")
            if (!logDir.exists()) {
                logDir.mkdirs()
            }

            val timeStamp = SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(Date())
            val logFile = File(logDir, "px5_crash_$timeStamp.log")

            // Get the full stack trace as a string
            val sw = StringWriter()
            val pw = PrintWriter(sw)
            throwable.printStackTrace(pw)
            val stackTrace = sw.toString()

            // Write the crash report
            logFile.bufferedWriter().use { writer ->
                writer.write("==============================================================\n")
                writer.write("PX5 EMULATOR CRASH LOG\n")
                writer.write("==============================================================\n")
                writer.write("Time: ${SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US).format(Date())}\n")
                writer.write("Thread: ${thread.name} (id=${thread.id})\n")
                writer.write("Process PID: ${android.os.Process.myPid()}\n")
                writer.write("Exception: ${throwable.javaClass.name}\n")
                writer.write("Message: ${throwable.message ?: "(none)"}\n")
                writer.write("\nStack trace:\n")
                writer.write(stackTrace)

                // If there's a cause, print it too
                var cause = throwable.cause
                var depth = 0
                while (cause != null && depth < 10) {
                    depth++
                    writer.write("\nCaused by (depth=$depth): ${cause.javaClass.name}: ${cause.message}\n")
                    val causeSw = StringWriter()
                    val causePw = PrintWriter(causeSw)
                    cause.printStackTrace(causePw)
                    writer.write(causeSw.toString())
                    cause = cause.cause
                }

                writer.write("\n==============================================================\n")
                writer.write("END OF CRASH REPORT\n")
                writer.write("==============================================================\n")
            }

            Log.i(TAG, "Crash log written to: ${logFile.absolutePath}")

            // Also update the "latest" crash log (single file, always
            // points to the most recent crash — easy for the user to find)
            val latestFile = File(logDir, "px5_crash_latest.log")
            logFile.copyTo(latestFile, overwrite = true)

        } catch (e: Exception) {
            Log.e(TAG, "Failed to write crash log file: ${e.message}", e)
        }
    }

    companion object {
        private const val TAG = "PX5Application"

        lateinit var instance: PX5Application
            private set

        /**
         * Returns the directory where crash logs are stored.
         * Useful for a settings screen that lets the user view/share logs.
         */
        fun getLogDirectory(context: Context): File {
            return File(context.getExternalFilesDir(null), "logs").also {
                if (!it.exists()) it.mkdirs()
            }
        }

        /**
         * Returns the contents of the latest crash log (or empty string
         * if no crash has been recorded).
         */
        fun getLatestCrashLog(context: Context): String {
            val logFile = File(getLogDirectory(context), "px5_crash_latest.log")
            return if (logFile.exists()) {
                logFile.readText()
            } else {
                "No crash logs recorded."
            }
        }

        /**
         * Lists all crash log files, newest first.
         */
        fun listCrashLogs(context: Context): List<File> {
            val dir = getLogDirectory(context)
            return dir.listFiles()
                ?.filter { it.isFile && it.name.startsWith("px5_crash_") && it.name.endsWith(".log") }
                ?.sortedByDescending { it.lastModified() }
                ?: emptyList()
        }

        /**
         * Deletes all crash log files.
         */
        fun clearAllCrashLogs(context: Context) {
            val dir = getLogDirectory(context)
            dir.listFiles()?.forEach { f ->
                if (f.isFile && f.name.startsWith("px5_crash_")) f.delete()
            }
        }

        // ---- Backward-compat with PS5SettingsScreen.kt ----
        // The settings screen calls getCrashLogs/clearCrashLogs/logSystemEvent
        // from the old PX5 API. Bridge them to the new system.

        fun getCrashLogs(context: Context): String {
            val logs = listCrashLogs(context)
            if (logs.isEmpty()) return "No crash or error logs recorded."
            val sb = StringBuilder(8192)
            for (f in logs) {
                sb.append("===== ").append(f.name).append(" (").append(f.length()).append(" bytes) =====\n")
                try {
                    sb.append(f.readText())
                } catch (e: Exception) {
                    sb.append("(could not read: ${e.message})\n")
                }
                sb.append("\n\n")
                if (sb.length > 200_000) {
                    sb.append("... (truncated, see files on disk for full logs)\n")
                    break
                }
            }
            return sb.toString()
        }

        fun clearCrashLogs(context: Context) {
            clearAllCrashLogs(context)
        }

        fun logSystemEvent(context: Context, tag: String, message: String) {
            val timeStamp = SimpleDateFormat("HH:mm:ss", Locale.US).format(Date())
            Log.i(tag, "[$timeStamp] $message")
        }
    }
}
