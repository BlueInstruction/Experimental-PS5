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
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
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

/**
 * EmuLogOverlay — the IN-GAME log viewer (v1.61).
 *
 * SharpDroid-style: a translucent terminal panel that floats OVER the
 * running game inside the emulation stage. It is NOT a navigation
 * destination — the stage composable stays mounted, the render surface
 * stays attached, the emulator keeps running. The old behaviour
 * (navigate to the Logs screen) disposed the stage and re-ran the whole
 * boot pipeline on return: the game reloaded every time the player peeked
 * at the log. This panel is the fix.
 *
 * Fed by the same three real sinks as the out-of-game Logs screen
 * (Px5LogTail); nothing invented, absent file = explicit empty state.
 */
@Composable
fun EmuLogOverlay(
    onDismiss: () -> Unit
) {
    val context = LocalContext.current
    var selectedSink by remember { mutableIntStateOf(1) }   // Engine first in-game
    var logText by remember { mutableStateOf("") }
    var autoRefresh by remember { mutableStateOf(true) }
    val sinks = Px5LogTail.SINK_NAMES

    LaunchedEffect(selectedSink) {
        logText = Px5LogTail.read(context, selectedSink, cap = 48_000)
    }
    LaunchedEffect(autoRefresh, selectedSink) {
        while (autoRefresh) {
            delay(2000)
            logText = Px5LogTail.read(context, selectedSink, cap = 48_000)
        }
    }

    val lines = remember(logText) { logText.split('\n') }
    val listState = rememberLazyListState()
    // Follow the tail unless the reader scrolled up to inspect history.
    // Observed through snapshotFlow on the list's OWN layout info, so it
    // never holds a stale line list.
    var followTail by remember { mutableStateOf(true) }
    LaunchedEffect(listState) {
        snapshotFlow {
            val info = listState.layoutInfo
            val last = info.visibleItemsInfo.lastOrNull()?.index ?: 0
            info.totalItemsCount == 0 || last >= info.totalItemsCount - 3
        }.collect { followTail = it }
    }
    LaunchedEffect(lines.size) {
        if (followTail && lines.isNotEmpty()) {
            listState.scrollToItem(lines.size - 1)
        }
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .fillMaxHeight(0.62f)
            .background(Color(0xF2101014))
            .border(1.dp, Color(0xFF2E8CFF).copy(alpha = 0.35f))
    ) {
        // ---- header: sink chips + live dot + copy + close -------------------
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .background(Color(0xFF15181E))
                .padding(horizontal = 10.dp, vertical = 4.dp)
        ) {
            Text(
                "Log",
                color = Color(0xFFF2F5FA),
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = TitilliumFontFamily
            )
            Spacer(Modifier.width(10.dp))
            sinks.forEachIndexed { i, name ->
                val sel = i == selectedSink
                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .padding(end = 6.dp)
                        .clip(RoundedCornerShape(12.dp))
                        .background(
                            if (sel) Color(0xFF0070D1).copy(alpha = 0.55f)
                            else Color.White.copy(alpha = 0.07f)
                        )
                        .border(
                            1.dp,
                            if (sel) Color(0xFF2E8CFF) else Color.White.copy(alpha = 0.12f),
                            RoundedCornerShape(12.dp)
                        )
                        .clickable { selectedSink = i }
                        .padding(horizontal = 10.dp, vertical = 3.dp)
                ) {
                    Text(
                        name,
                        color = if (sel) Color.White else Color(0xFF9BA7BC),
                        fontSize = 11.sp,
                        fontWeight = if (sel) FontWeight.Bold else FontWeight.Medium,
                        fontFamily = TitilliumFontFamily
                    )
                }
            }
            Spacer(Modifier.weight(1f))
            Box(
                modifier = Modifier
                    .size(7.dp)
                    .clip(CircleShape)
                    .background(if (autoRefresh) Color(0xFF69F0AE) else Color(0xFF666B76))
            )
            Spacer(Modifier.width(4.dp))
            Text(
                "live",
                color = if (autoRefresh) Color(0xFF69F0AE) else Color(0xFF9BA7BC),
                fontSize = 10.sp,
                fontFamily = FontFamily.Monospace,
                modifier = Modifier.clickable { autoRefresh = !autoRefresh }
            )
            Spacer(Modifier.width(6.dp))
            IconButton(
                onClick = {
                    val send = Intent(Intent.ACTION_SEND).apply {
                        type = "text/plain"
                        putExtra(Intent.EXTRA_SUBJECT, "PSX5 in-game log")
                        putExtra(Intent.EXTRA_TEXT, logText.take(180_000))
                    }
                    context.startActivity(Intent.createChooser(send, "Share log"))
                },
                modifier = Modifier.size(30.dp)
            ) {
                Icon(
                    Icons.Default.ContentCopy, "Share",
                    tint = Color(0xFFF2F5FA), modifier = Modifier.size(15.dp)
                )
            }
            IconButton(
                onClick = onDismiss,
                modifier = Modifier.size(30.dp)
            ) {
                Icon(
                    Icons.Default.Close, "Close",
                    tint = Color(0xFFF2F5FA), modifier = Modifier.size(17.dp)
                )
            }
        }

        // ---- terminal --------------------------------------------------------
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.55f))
        ) {
            if (logText.isBlank()) {
                Text(
                    text = when (selectedSink) {
                        0 -> "(no app log yet)"
                        1 -> "(no engine log yet)"
                        else -> "(no crash dumps)"
                    },
                    color = Color(0xFF9BA7BC),
                    fontSize = 11.sp,
                    fontFamily = FontFamily.Monospace,
                    modifier = Modifier
                        .align(Alignment.Center)
                        .padding(16.dp)
                )
            } else {
                LazyColumn(
                    state = listState,
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(horizontal = 10.dp, vertical = 6.dp)
                ) {
                    items(lines) { line ->
                        Text(
                            text = line,
                            fontSize = 10.sp,
                            lineHeight = 13.sp,
                            color = Px5LogTail.lineColor(line),
                            fontFamily = FontFamily.Monospace
                        )
                    }
                }
            }
        }
    }
}
