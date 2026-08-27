package com.px5.emulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.core.FexCoreWrapper
import kotlinx.coroutines.delay

/**
 * DeckShell — Steam-Deck-OS-flavored status components fed by REAL engine
 * telemetry (JNI polling), PS5 accents preserved.
 */
@Composable
fun EngineStatusStrip(
    fexCoreWrapper: FexCoreWrapper?,
    cpuStatus: String
) {
    var vulkanLine by remember { mutableStateOf("GPU: probing…") }
    var renderLine by remember { mutableStateOf("renderer: idle") }

    LaunchedEffect(Unit) {
        while (true) {
            try {
                if (fexCoreWrapper != null) {
                    vulkanLine = runCatching { fexCoreWrapper.nativeGetVulkanSummary() }
                        .getOrDefault("GPU: n/a")
                    renderLine = runCatching { fexCoreWrapper.nativeGetRenderStats() }
                        .getOrDefault("renderer: n/a")
                }
            } catch (_: Throwable) {}
            delay(1200)
        }
    }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 40.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(Color(0xFF121A24))
            .padding(horizontal = 14.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Dot(color = if (cpuStatus == "Ready") Color(0xFF69F0AE)
                    else if (cpuStatus.startsWith("Error")) Color(0xFFFF5252)
                    else Color(0xFFFFC400))
        Column {
            Text("CPU BRIDGE", fontSize = 9.sp, color = PS5TextSecondary,
                 fontWeight = FontWeight.Bold)
            Text(cpuStatus, fontSize = 11.sp, color = PS5TextPrimary)
        }

        Box(Modifier.width(1.dp).height(26.dp)
                .background(Color.White.copy(alpha = 0.08f)))

        Dot(color = Color(0xFF7DD3FC))
        Column {
            Text("VULKAN", fontSize = 9.sp, color = PS5TextSecondary,
                 fontWeight = FontWeight.Bold)
            Text(vulkanLine.take(46), fontSize = 11.sp, color = PS5TextPrimary)
        }

        Box(Modifier.width(1.dp).height(26.dp)
                .background(Color.White.copy(alpha = 0.08f)))

        Dot(color = Color(0xFFE2C74B))
        Column {
            Text("RENDER LOOP", fontSize = 9.sp, color = PS5TextSecondary,
                 fontWeight = FontWeight.Bold)
            Text(renderLine.take(46), fontSize = 11.sp, color = PS5TextPrimary)
        }
    }
}

@Composable
private fun Dot(color: Color) {
    Box(
        modifier = Modifier
            .size(8.dp)
            .clip(CircleShape)
            .background(color)
    )
}
