package com.px5.emulator.ui

import androidx.compose.ui.graphics.Color
import com.px5.emulator.PX5Application
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

/**
 * Shared log-tail plumbing for the two log surfaces (the out-of-game Logs
 * screen and the in-game overlay). Both read the SAME three real sinks and
 * only the newest bytes; nothing here invents content.
 *
 *   App     <logs>/px5_diagnostic.log   (app-level event/state/diagnostic lines)
 *   Engine  <logs>/px5_main.log(.1-.4)  (native rotating logger, 5x1MB)
 *   Crashes <logs>/px5_crash_latest.log
 */
object Px5LogTail {

    val SINK_NAMES = listOf("App", "Engine", "Crashes")

    /** Sink id -> (file, byte cap). Engine resolves the rotating set. */
    fun sinkFile(dir: File, sink: Int): Pair<File, Int> = when (sink) {
        0 -> File(dir, "px5_diagnostic.log") to 96_000
        1 -> latestEngineLog(dir) to 96_000
        else -> File(dir, "px5_crash_latest.log") to 96_000
    }

    /** Reads the newest bytes of the selected sink on the IO dispatcher. */
    suspend fun read(context: android.content.Context, sink: Int, cap: Int = 96_000): String =
        withContext(Dispatchers.IO) {
            val dir = PX5Application.getLogDirectory(context)
            val (f, realCap) = sinkFile(dir, sink)
            runCatching { if (f.isFile) tail(f, realCap) else "" }.getOrDefault("")
        }

    /** One compact "app=… engine=… crashDumps=…" size line for headers. */
    fun sizeLine(context: android.content.Context): String = runCatching {
        val dir = PX5Application.getLogDirectory(context)
        val diag = File(dir, "px5_diagnostic.log")
        val eng = latestEngineLog(dir)
        buildString {
            append("app=")
            append(if (diag.isFile) humanSize(diag.length()) else "—")
            append("  engine=")
            append(if (eng.isFile) humanSize(eng.length()) else "—")
            append("  crashDumps=").append(PX5Application.listCrashLogs(context).size)
        }
    }.getOrDefault("")

    /** Engine sink: the newest file of the px5_main.log rotating set. */
    fun latestEngineLog(dir: File): File {
        val newest = File(dir, "px5_main.log")
        if (newest.isFile) return newest
        // Rotation may have advanced; find the freshest .N shard.
        return (1..4).map { File(dir, "px5_main.log.$it") }
            .filter { it.isFile }
            .maxByOrNull { it.lastModified() }
            ?: newest
    }

    /** Reads at most [cap] bytes from the end of [f]; never throws. */
    fun tail(f: File, cap: Int): String {
        val len = f.length()
        if (len <= cap) return f.readText()
        val start = len - cap
        val bytes = ByteArray(cap)
        var read = 0
        f.inputStream().use { ins ->
            ins.skip(start)
            while (read < cap) {
                val r = ins.read(bytes, read, cap - read)
                if (r < 0) break
                read += r
            }
        }
        // Drop the first (almost certainly partial) line.
        return String(bytes, 0, read).substringAfter('\n', "")
    }

    fun humanSize(b: Long): String = when {
        b >= 1L shl 20 -> "%.1f MB".format(b / 1048576.0)
        b >= 1L shl 10 -> "%.1f KB".format(b / 1024.0)
        else -> "${b}B"
    }

    /** Evidence coloring: failures red, passes green, warnings amber. */
    fun lineColor(line: String): Color = when {
        line.contains("[FAIL]", true) || line.contains("ERROR", true) ||
                line.startsWith("VERDICT: FAIL", true) -> Color(0xFFFF5252)
        line.contains("[PASS]", true) || line.startsWith("VERDICT: PASS", true) ||
                line.contains("verified", true) -> Color(0xFF69F0AE)
        line.contains("WARN", true) -> Color(0xFFFFC400)
        else -> Color(0xFFB9C4D6)
    }
}
