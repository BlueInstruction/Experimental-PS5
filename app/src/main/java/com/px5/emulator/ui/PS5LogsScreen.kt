package com.px5.emulator.ui

import android.content.Intent
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Share
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

/**
 * PS5LogsScreen — live log viewer in the spirit of Dolphin's log page and
 * sharpdroid's on-device log window, fed ONLY by the three real sinks:
 *
 *   App     <logs>/px5_diagnostic.log   (app-level event/state/diagnostic lines)
 *   Engine  <logs>/px5_main.log(.1-.4)  (native rotating logger, 5x1MB)
 *   Crashes <logs>/px5_crash_*.log      (uncaught-exception dumps)
 *
 * Nothing here invents content: an absent file renders an explicit
 * "(no log file yet)" state. Auto-refresh tails the selected sink every
 * 2 s; Share hands the visible tail to the system share sheet.
 */
@Composable
fun PS5LogsScreen(
    onBackClick: () -> Unit
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    // v1.37 — when opened from an ACTIVE game session (in-game menu →
    // "Diagnostics & logs") this screen must stay LANDSCAPE: the log flip
    // to portrait under a running game recreates the display surface and
    // crashes it. Outside a session the shell orientation preference
    // governs, so this effect only acts on an active session.
    DisposableEffect(Unit) {
        if (com.px5.emulator.core.Px5Settings.isGameSessionActive()) {
            (context as? android.app.Activity)?.requestedOrientation =
                android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        }
        onDispose {}
    }

    var selectedSink by remember { mutableIntStateOf(0) }
    var autoRefresh by remember { mutableStateOf(true) }
    var logText by remember { mutableStateOf("loading…") }
    var fileSizes by remember { mutableStateOf("") }
    var busy by remember { mutableStateOf(false) }

    val sinks = Px5LogTail.SINK_NAMES

    suspend fun reload() {
        busy = true
        val text = Px5LogTail.read(context, selectedSink)
        logText = text
        fileSizes = Px5LogTail.sizeLine(context)
        busy = false
    }

    LaunchedEffect(selectedSink) { reload() }
    LaunchedEffect(autoRefresh, selectedSink) {
        while (autoRefresh) {
            delay(2000)
            reload()
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF0B0E13))
            .windowInsetsPadding(WindowInsets.statusBars)
            .padding(horizontal = 16.dp, vertical = 12.dp)
    ) {
        // ---- header -------------------------------------------------------
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth()
        ) {
            IconButton(
                onClick = onBackClick,
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.1f))
            ) {
                Icon(
                    Icons.AutoMirrored.Filled.ArrowBack,
                    contentDescription = "Back",
                    tint = Color(0xFFF2F5FA)
                )
            }
            Spacer(Modifier.width(12.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    "Logs",
                    color = Color(0xFFF2F5FA),
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = TitilliumFontFamily
                )
                Text(
                    fileSizes,
                    color = Color(0xFF9BA7BC),
                    fontSize = 11.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
            IconButton(
                onClick = { scope.launch { reload() } },
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.1f))
            ) {
                if (busy) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(18.dp),
                        strokeWidth = 2.dp,
                        color = Color(0xFF2E8CFF)
                    )
                } else {
                    Icon(Icons.Default.Refresh, "Refresh",
                         tint = Color(0xFFF2F5FA), modifier = Modifier.size(20.dp))
                }
            }
            IconButton(
                onClick = {
                    val send = Intent(Intent.ACTION_SEND).apply {
                        type = "text/plain"
                        putExtra(Intent.EXTRA_SUBJECT, "PX5 ${sinks[selectedSink]} log")
                        putExtra(Intent.EXTRA_TEXT, logText.take(180_000))
                    }
                    context.startActivity(
                        Intent.createChooser(send, "Share log")
                    )
                },
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.1f))
            ) {
                Icon(Icons.Default.Share, "Share",
                     tint = Color(0xFFF2F5FA), modifier = Modifier.size(20.dp))
            }
        }

        Spacer(Modifier.height(10.dp))

        // ---- sink tabs -----------------------------------------------------
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            sinks.forEachIndexed { i, name ->
                val sel = i == selectedSink
                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .clip(RoundedCornerShape(16.dp))
                        .background(
                            if (sel) Color(0xFF0070D1).copy(alpha = 0.5f)
                            else Color.White.copy(alpha = 0.07f)
                        )
                        .border(
                            1.dp,
                            if (sel) Color(0xFF2E8CFF) else Color.White.copy(alpha = 0.12f),
                            RoundedCornerShape(16.dp)
                        )
                        .clickable {
                            selectedSink = i
                            logText = "loading…"
                        }
                        .padding(horizontal = 16.dp, vertical = 7.dp)
                ) {
                    Text(
                        name,
                        color = if (sel) Color.White else Color(0xFF9BA7BC),
                        fontSize = 12.sp,
                        fontWeight = if (sel) FontWeight.Bold else FontWeight.Medium,
                        fontFamily = TitilliumFontFamily
                    )
                }
            }
            Spacer(Modifier.weight(1f))
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier
                    .clip(RoundedCornerShape(16.dp))
                    .clickable { autoRefresh = !autoRefresh }
                    .padding(horizontal = 8.dp, vertical = 7.dp)
            ) {
                Box(
                    modifier = Modifier
                        .size(9.dp)
                        .clip(CircleShape)
                        .background(
                            if (autoRefresh) Color(0xFF69F0AE) else Color(0xFF666B76)
                        )
                )
                Spacer(Modifier.width(6.dp))
                Text(
                    "live",
                    color = if (autoRefresh) Color(0xFF69F0AE) else Color(0xFF9BA7BC),
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace
                )
            }
        }

        Spacer(Modifier.height(10.dp))

        // ---- terminal --------------------------------------------------------
        val lines = remember(logText) { logText.split('\n') }
        val listState = rememberLazyListState()
        LaunchedEffect(lines.size) {
            if (lines.isNotEmpty()) listState.animateScrollToItem(lines.size - 1)
        }
        Box(
            modifier = Modifier
                .fillMaxSize()
                .clip(RoundedCornerShape(14.dp))
                .background(Color.Black.copy(alpha = 0.65f))
                .border(1.dp, Color(0xFF0070D1).copy(alpha = 0.4f), RoundedCornerShape(14.dp))
        ) {
            if (logText.isBlank()) {
                Text(
                    text = when (selectedSink) {
                        0 -> "(no app log yet — px5_diagnostic.log appears once the app logs events)"
                        1 -> "(no engine log yet — px5_main.log appears once the native engine initializes its logger)"
                        else -> "(no crash dumps — px5_crash_latest.log appears after an uncaught exception)"
                    },
                    color = Color(0xFF9BA7BC),
                    fontSize = 12.sp,
                    fontFamily = FontFamily.Monospace,
                    modifier = Modifier
                        .align(Alignment.Center)
                        .padding(24.dp)
                )
            } else {
                LazyColumn(
                    state = listState,
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(12.dp)
                ) {
                    items(lines) { line ->
                        Text(
                            text = line,
                            fontSize = 11.sp,
                            lineHeight = 15.sp,
                            color = Px5LogTail.lineColor(line),
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }
    }
}

/** Engine sink helper moved to Px5LogTail (shared with the in-game overlay). */

// ---------------------------------------------------------------------------
// (tail / humanSize / lineColor live in Px5LogTail.kt — one implementation
// for the Logs screen and the in-game overlay.)
// ---------------------------------------------------------------------------
