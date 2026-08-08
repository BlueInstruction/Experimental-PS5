package com.px5.emulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Refresh
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
import com.px5.emulator.core.FexCoreWrapper

data class TurnipDriverProfile(
    val id: String,
    val name: String,
    val version: String,
    val description: String,
    val isSystem: Boolean = false
)

@Composable
fun PS5TurnipDriverSheet(
    soundManager: SoundManager,
    fexCoreWrapper: FexCoreWrapper,
    onImportCustomDriverClick: () -> Unit,
    onDismiss: () -> Unit
) {
    val defaultDrivers = remember {
        listOf(
            TurnipDriverProfile(
                id = "turnip_24_1",
                name = "Turnip Mesa 24.1.0-devel",
                version = "v24.1.0 (a6xx/a7xx)",
                description = "Recommended for PS5 GNM/GNMX translation. Full Vulkan 1.3 + BCn texture extensions."
            ),
            TurnipDriverProfile(
                id = "turnip_23_3",
                name = "Turnip Mesa 23.3.0 Stable",
                version = "v23.3.0 (a6xx)",
                description = "High stability profile for Adreno 640/650/660 GPUs."
            ),
            TurnipDriverProfile(
                id = "system_adreno",
                name = "System Qualcomm Adreno Driver",
                version = "Native System Driver",
                description = "Default device driver without Turnip libadrenotools injection.",
                isSystem = true
            )
        )
    }

    var selectedDriverId by remember { mutableStateOf("turnip_24_1") }
    var bcnEnabled by remember { mutableStateOf(true) }
    var pipelineCacheEnabled by remember { mutableStateOf(true) }
    var kgslHookEnabled by remember { mutableStateOf(true) }

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
            .background(Color(0xFF141A24))
            .border(1.dp, Color.White.copy(alpha = 0.15f), RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
            .padding(28.dp)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            // Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    imageVector = Icons.Default.PlayArrow,
                    contentDescription = "Turnip",
                    tint = PS5AccentGlow,
                    modifier = Modifier.size(26.dp)
                )

                Spacer(modifier = Modifier.width(10.dp))

                Column {
                    Text(
                        text = "Turnip Vulkan & libadrenotools Manager",
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        color = PS5TextPrimary,
                        fontFamily = TitilliumFontFamily
                    )
                    Text(
                        text = "Custom Mesa Vulkan Driver Injection for Adreno GPUs (Rule 5 & 9 Compliant)",
                        fontSize = 12.sp,
                        color = PS5TextSecondary,
                        fontFamily = TitilliumFontFamily
                    )
                }

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

            // Driver Selection List
            Text(
                text = "Select Turnip Driver Profile:",
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                color = PS5TextPrimary,
                fontFamily = TitilliumFontFamily
            )

            Spacer(modifier = Modifier.height(10.dp))

            LazyColumn(
                verticalArrangement = Arrangement.spacedBy(10.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 200.dp)
            ) {
                items(defaultDrivers) { profile ->
                    val isSelected = profile.id == selectedDriverId
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(12.dp))
                            .background(if (isSelected) PS5AccentBlue.copy(alpha = 0.3f) else Color.White.copy(alpha = 0.05f))
                            .border(
                                width = if (isSelected) 1.5.dp else 0.dp,
                                color = if (isSelected) PS5AccentGlow else Color.Transparent,
                                shape = RoundedCornerShape(12.dp)
                            )
                            .clickable {
                                soundManager.playNavigationSound()
                                selectedDriverId = profile.id
                                fexCoreWrapper.nativeInitAdrenotools(
                                    "/sdcard/PX5/Drivers/${profile.id}",
                                    profile.name,
                                    "libadrenotools.so"
                                )
                            }
                            .padding(14.dp)
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Column(modifier = Modifier.weight(1f)) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Text(
                                        text = profile.name,
                                        fontSize = 15.sp,
                                        fontWeight = FontWeight.Bold,
                                        color = PS5TextPrimary,
                                        fontFamily = TitilliumFontFamily
                                    )
                                    Spacer(modifier = Modifier.width(8.dp))
                                    Text(
                                        text = profile.version,
                                        fontSize = 11.sp,
                                        color = PS5AccentGlow,
                                        fontFamily = TitilliumFontFamily
                                    )
                                }
                                Text(
                                    text = profile.description,
                                    fontSize = 12.sp,
                                    color = PS5TextSecondary,
                                    fontFamily = TitilliumFontFamily
                                )
                            }
                            if (isSelected) {
                                Icon(
                                    imageVector = Icons.Default.Check,
                                    contentDescription = "Selected",
                                    tint = PS5AccentGlow,
                                    modifier = Modifier.size(20.dp)
                                )
                            }
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(18.dp))

            // Turnip Driver Toggles
            Text(
                text = "libadrenotools & Turnip Engine Settings:",
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                color = PS5TextPrimary,
                fontFamily = TitilliumFontFamily
            )

            Spacer(modifier = Modifier.height(8.dp))

            Card(
                colors = CardDefaults.cardColors(containerColor = Color.White.copy(alpha = 0.05f)),
                shape = RoundedCornerShape(16.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    // Toggle 1: BCn Texture Decoding
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = "Turnip BCn Texture Decoding",
                                fontSize = 13.sp,
                                fontWeight = FontWeight.SemiBold,
                                color = PS5TextPrimary,
                                fontFamily = TitilliumFontFamily
                            )
                            Text(
                                text = "Hardware ASTC/BC1-BC7 texture unpacking for GNM shaders",
                                fontSize = 11.sp,
                                color = PS5TextSecondary,
                                fontFamily = TitilliumFontFamily
                            )
                        }
                        Switch(
                            checked = bcnEnabled,
                            onCheckedChange = {
                                bcnEnabled = it
                                fexCoreWrapper.nativeSetTurnipBcnTextureSupport(it)
                            },
                            colors = SwitchDefaults.colors(
                                checkedThumbColor = Color.White,
                                checkedTrackColor = PS5AccentBlue
                            )
                        )
                    }

                    HorizontalDivider(
                        color = Color.White.copy(alpha = 0.08f),
                        modifier = Modifier.padding(vertical = 8.dp)
                    )

                    // Toggle 2: Vulkan Pipeline Cache
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = "Vulkan Pipeline Caching",
                                fontSize = 13.sp,
                                fontWeight = FontWeight.SemiBold,
                                color = PS5TextPrimary,
                                fontFamily = TitilliumFontFamily
                            )
                            Text(
                                text = "Store compiled SPIR-V shaders in /sdcard/PX5/Cache",
                                fontSize = 11.sp,
                                color = PS5TextSecondary,
                                fontFamily = TitilliumFontFamily
                            )
                        }
                        Switch(
                            checked = pipelineCacheEnabled,
                            onCheckedChange = {
                                pipelineCacheEnabled = it
                                fexCoreWrapper.nativeSetTurnipPipelineCaching(it)
                            },
                            colors = SwitchDefaults.colors(
                                checkedThumbColor = Color.White,
                                checkedTrackColor = PS5AccentBlue
                            )
                        )
                    }

                    HorizontalDivider(
                        color = Color.White.copy(alpha = 0.08f),
                        modifier = Modifier.padding(vertical = 8.dp)
                    )

                    // Toggle 3: KGSL Hook
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = "libadrenotools KGSL FD Hooking",
                                fontSize = 13.sp,
                                fontWeight = FontWeight.SemiBold,
                                color = PS5TextPrimary,
                                fontFamily = TitilliumFontFamily
                            )
                            Text(
                                text = "Bypass Android driver permission limits for custom .so loading",
                                fontSize = 11.sp,
                                color = PS5TextSecondary,
                                fontFamily = TitilliumFontFamily
                            )
                        }
                        Switch(
                            checked = kgslHookEnabled,
                            onCheckedChange = { kgslHookEnabled = it },
                            colors = SwitchDefaults.colors(
                                checkedThumbColor = Color.White,
                                checkedTrackColor = PS5AccentBlue
                            )
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(20.dp))

            // Action Row
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                OutlinedButton(
                    onClick = {
                        soundManager.playActivationSound()
                        onImportCustomDriverClick()
                    },
                    shape = RoundedCornerShape(16.dp),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = PS5TextPrimary),
                    border = ButtonDefaults.outlinedButtonBorder.copy(brush = androidx.compose.ui.graphics.SolidColor(Color.White.copy(alpha = 0.3f))),
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(imageVector = Icons.Default.Refresh, contentDescription = "Import", modifier = Modifier.size(16.dp))
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = "Import Turnip ZIP Driver",
                        fontSize = 13.sp,
                        fontFamily = TitilliumFontFamily
                    )
                }

                Button(
                    onClick = {
                        soundManager.playActivationSound()
                        onDismiss()
                    },
                    shape = RoundedCornerShape(16.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = PS5AccentBlue, contentColor = Color.White),
                    modifier = Modifier.weight(1f)
                ) {
                    Text(
                        text = "Apply Driver Profile",
                        fontWeight = FontWeight.Bold,
                        fontSize = 13.sp,
                        fontFamily = TitilliumFontFamily
                    )
                }
            }
        }
    }
}
