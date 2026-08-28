package com.px5.emulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.SoundManager

@Composable
fun PS5ControlCenterSheet(
    soundManager: SoundManager,
    fexCoreStatus: String,
    fexCoreWrapper: com.px5.emulator.core.FexCoreWrapper? = null,
    onDismiss: () -> Unit,
    onRestartRequested: () -> Unit,
    modifier: Modifier = Modifier
) {
    var isMuted by remember { mutableStateOf(!soundManager.isSoundEnabled) }
    var bgMusicEnabled by remember { mutableStateOf(soundManager.isBgMusicEnabled) }
    // Real GPU/runtime facts, polled once when the sheet opens.
    var vulkanLine by remember { mutableStateOf("") }
    var driverLine by remember { mutableStateOf("") }
    LaunchedEffect(Unit) {
        fexCoreWrapper?.let { w ->
            vulkanLine = runCatching { w.nativeGetVulkanSummary() }.getOrDefault("")
            driverLine = runCatching { w.nativeGetDriverManagerSummary() }.getOrDefault("")
        }
    }

    Box(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
            .background(Color(0xFF141A24))
            .border(1.dp, Color.White.copy(alpha = 0.15f), RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
            .padding(28.dp)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            // Header Row
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Control Center",
                    fontSize = 22.sp,
                    fontWeight = FontWeight.Bold,
                    color = PS5TextPrimary,
                    fontFamily = TitilliumFontFamily
                )

                Spacer(modifier = Modifier.weight(1f))

                IconButton(
                    onClick = onDismiss,
                    modifier = Modifier
                        .size(36.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.1f))
                ) {
                    Icon(
                        imageVector = Icons.Default.Close,
                        contentDescription = "Close",
                        tint = PS5TextPrimary,
                        modifier = Modifier.size(18.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(20.dp))

            // Quick Control Tiles Row
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Sound Effects Toggle
                ControlCenterTile(
                    title = "Sound Effects",
                    statusText = if (!isMuted) "Enabled" else "Muted",
                    iconVector = Icons.Default.Notifications,
                    isActive = !isMuted,
                    onClick = {
                        isMuted = !isMuted
                        soundManager.isSoundEnabled = !isMuted
                        if (!isMuted) soundManager.playNavigationSound()
                    },
                    modifier = Modifier.weight(1f)
                )

                // Background Music Toggle
                ControlCenterTile(
                    title = "PS5 BGM",
                    statusText = if (bgMusicEnabled) "Playing" else "Off",
                    iconVector = Icons.Default.Refresh,
                    isActive = bgMusicEnabled,
                    onClick = {
                        bgMusicEnabled = !bgMusicEnabled
                        soundManager.isBgMusicEnabled = bgMusicEnabled
                        if (bgMusicEnabled) soundManager.playNavigationSound()
                    },
                    modifier = Modifier.weight(1f)
                )

                // Power Options
                ControlCenterTile(
                    title = "Rest Mode",
                    statusText = "Restart Core",
                    iconVector = Icons.Default.Settings,
                    isActive = false,
                    onClick = {
                        soundManager.playActivationSound()
                        onRestartRequested()
                    },
                    modifier = Modifier.weight(1f)
                )
            }

            Spacer(modifier = Modifier.height(20.dp))

            // System Status Card — real engine state only
            Card(
                colors = CardDefaults.cardColors(containerColor = Color.White.copy(alpha = 0.05f)),
                shape = RoundedCornerShape(16.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text(
                        text = "Engine Status",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        color = PS5AccentGlow,
                        fontFamily = TitilliumFontFamily
                    )
                    Spacer(modifier = Modifier.height(6.dp))
                    Text(
                        text = "CPU bridge: $fexCoreStatus",
                        fontSize = 12.sp,
                        color = PS5TextPrimary,
                        fontFamily = TitilliumFontFamily
                    )
                    if (vulkanLine.isNotBlank()) {
                        Text(
                            text = vulkanLine,
                            fontSize = 12.sp,
                            color = PS5TextSecondary,
                            fontFamily = TitilliumFontFamily
                        )
                    }
                    if (driverLine.isNotBlank()) {
                        Text(
                            text = driverLine,
                            fontSize = 11.sp,
                            color = PS5TextSecondary,
                            fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun ControlCenterTile(
    title: String,
    statusText: String,
    iconVector: androidx.compose.ui.graphics.vector.ImageVector,
    isActive: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .height(100.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(if (isActive) PS5AccentBlue.copy(alpha = 0.25f) else Color.White.copy(alpha = 0.06f))
            .border(
                1.dp,
                if (isActive) PS5AccentGlow else Color.White.copy(alpha = 0.15f),
                RoundedCornerShape(16.dp)
            )
            .clickable { onClick() }
            .padding(14.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            Icon(
                imageVector = iconVector,
                contentDescription = title,
                tint = if (isActive) PS5AccentGlow else PS5TextPrimary,
                modifier = Modifier.size(24.dp)
            )
            Spacer(modifier = Modifier.weight(1f))
            Text(
                text = title,
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
                color = PS5TextPrimary,
                fontFamily = TitilliumFontFamily
            )
            Text(
                text = statusText,
                fontSize = 11.sp,
                color = PS5TextSecondary,
                fontFamily = TitilliumFontFamily
            )
        }
    }
}
