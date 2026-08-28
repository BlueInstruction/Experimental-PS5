package com.px5.emulator.ui

import android.os.StatFs
import java.io.File
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListScope
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.SoundManager
import com.px5.emulator.core.FexCoreWrapper
import com.px5.emulator.core.Px5Settings
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

/**
 * Settings — rebuilt for honesty and real layouts.
 *
 * Previous version shipped classic vibe-code artifacts: a fixed 260dp
 * sidebar that clipped portrait phones, "Console SSD Storage 825 GB",
 * "Battery Level 85% (Good)" (a hardcoded fake), "Constitution
 * Compliance 15/15", a controller listed as connected that was never
 * probed. All of that is gone. Every value now comes from the system,
 * the engine, or is simply not shown. Category navigation switches to a
 * horizontal chip row on narrow (portrait) screens and keeps a sidebar
 * on wide (landscape) ones.
 */
private data class SettingsCategory(val title: String, val icon: ImageVector)

@Composable
fun PS5SettingsScreen(
    soundManager: SoundManager,
    fexCoreStatus: String,
    fexCoreWrapper: FexCoreWrapper? = null,
    onImportFileClick: () -> Unit = {},
    onImportFolderClick: () -> Unit = {},
    onScanGamesClick: () -> Unit = {},
    onOpenTurnipManagerClick: () -> Unit = {},
    onBackClick: () -> Unit
) {
    var selectedCategory by remember { mutableStateOf(0) }

    val categories = listOf(
        SettingsCategory("System", Icons.Default.Info),
        SettingsCategory("Graphics", Icons.Default.PlayArrow),
        SettingsCategory("Audio", Icons.Default.VolumeUp),
        SettingsCategory("Storage & Games", Icons.Default.FolderOpen),
        SettingsCategory("GPU Drivers", Icons.Default.Memory),
        SettingsCategory("Diagnostics", Icons.Default.BugReport)
    )

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(PS5DarkBackground)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .windowInsetsPadding(WindowInsets.statusBars)
                .padding(horizontal = 20.dp, vertical = 16.dp)
        ) {
            // Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(
                    onClick = {
                        soundManager.playNavigationSound()
                        onBackClick()
                    },
                    modifier = Modifier
                        .size(44.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.1f))
                ) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Back",
                        tint = PS5TextPrimary
                    )
                }
                Spacer(modifier = Modifier.width(16.dp))
                Text(
                    text = "Settings",
                    fontSize = 26.sp,
                    fontWeight = FontWeight.Bold,
                    color = PS5TextPrimary,
                    fontFamily = TitilliumFontFamily
                )
            }

            Spacer(modifier = Modifier.height(20.dp))

            BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
                val wide = maxWidth >= 700.dp

                if (wide) {
                    Row(
                        modifier = Modifier.fillMaxSize(),
                        horizontalArrangement = Arrangement.spacedBy(20.dp)
                    ) {
                        // Sidebar (wide screens only)
                        Column(
                            modifier = Modifier
                                .width(230.dp)
                                .fillMaxHeight()
                                .verticalScroll(rememberScrollState()),
                            verticalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            categories.forEachIndexed { i, cat ->
                                SettingsCategoryTab(
                                    title = cat.title,
                                    icon = cat.icon,
                                    isSelected = selectedCategory == i,
                                    onClick = {
                                        selectedCategory = i
                                        soundManager.playNavigationSound()
                                    }
                                )
                            }
                        }
                        SettingsPanel(
                            category = selectedCategory,
                            soundManager = soundManager,
                            fexCoreStatus = fexCoreStatus,
                            fexCoreWrapper = fexCoreWrapper,
                            onImportFileClick = onImportFileClick,
                            onImportFolderClick = onImportFolderClick,
                            onScanGamesClick = onScanGamesClick,
                            onOpenTurnipManagerClick = onOpenTurnipManagerClick
                        )
                    }
                } else {
                    Column(modifier = Modifier.fillMaxSize()) {
                        // Chip row (portrait)
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .horizontalScroll(rememberScrollState()),
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            categories.forEachIndexed { i, cat ->
                                SettingsChip(
                                    title = cat.title,
                                    icon = cat.icon,
                                    isSelected = selectedCategory == i,
                                    onClick = {
                                        selectedCategory = i
                                        soundManager.playNavigationSound()
                                    }
                                )
                            }
                        }
                        Spacer(modifier = Modifier.height(14.dp))
                        SettingsPanel(
                            category = selectedCategory,
                            soundManager = soundManager,
                            fexCoreStatus = fexCoreStatus,
                            fexCoreWrapper = fexCoreWrapper,
                            onImportFileClick = onImportFileClick,
                            onImportFolderClick = onImportFolderClick,
                            onScanGamesClick = onScanGamesClick,
                            onOpenTurnipManagerClick = onOpenTurnipManagerClick
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun SettingsPanel(
    category: Int,
    soundManager: SoundManager,
    fexCoreStatus: String,
    fexCoreWrapper: FexCoreWrapper?,
    onImportFileClick: () -> Unit,
    onImportFolderClick: () -> Unit,
    onScanGamesClick: () -> Unit,
    onOpenTurnipManagerClick: () -> Unit
) {
    Card(
        colors = CardDefaults.cardColors(containerColor = Color.White.copy(alpha = 0.04f)),
        shape = RoundedCornerShape(18.dp),
        modifier = Modifier
            .fillMaxSize()
            .border(1.dp, Color.White.copy(alpha = 0.1f), RoundedCornerShape(18.dp))
    ) {
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            when (category) {
                0 -> systemSection(fexCoreStatus, fexCoreWrapper, soundManager)
                1 -> graphicsSection(fexCoreWrapper, soundManager, onOpenTurnipManagerClick)
                2 -> audioSection(soundManager)
                3 -> storageSection(onImportFileClick, onImportFolderClick, onScanGamesClick)
                4 -> driversSection(onOpenTurnipManagerClick)
                5 -> diagnosticsSection(soundManager)
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 0 — System: real facts + the real evidence pipeline
// ---------------------------------------------------------------------------

private fun LazyListScope.systemSection(
    fexCoreStatus: String,
    fexCoreWrapper: FexCoreWrapper?,
    soundManager: SoundManager
) {
    item {
        SettingsHeader("System")
        val appInfo = androidx.compose.ui.platform.LocalContext.current
            .packageManager.getPackageInfo(
                androidx.compose.ui.platform.LocalContext.current.packageName, 0
            )
        SettingsItemText("App version", appInfo.versionName ?: "?")
        SettingsItemText("CPU bridge", "FEXCore x86-64 → ARM64 (libpx5.so)")
        SettingsItemText("Bridge status", fexCoreStatus)
    }
    item {
        fexCoreWrapper?.let { wrapper ->
            var archSummary by remember { mutableStateOf("") }
            var testResult by remember { mutableStateOf<String?>(null) }
            LaunchedEffect(Unit) {
                try {
                    archSummary = wrapper.nativeGetArchitectureSummary()
                } catch (e: Exception) {
                    archSummary = ""
                }
            }

            if (archSummary.isNotBlank()) {
                SettingsSubHeader("Architecture subsystems")
                MonoReportBox(archSummary)
            }

            Spacer(modifier = Modifier.height(10.dp))
            Button(
                onClick = {
                    soundManager.playActivationSound()
                    try {
                        val pass = wrapper.nativeRunCpuConformanceTest()
                        testResult = if (pass) {
                            "PASSED: RAX=42 (mov eax,40; add eax,2; hlt) via ARM64 JIT"
                        } else {
                            "FAILED — see logcat (FEX tag)"
                        }
                    } catch (e: Exception) {
                        testResult = "Error running test: ${e.message}"
                    }
                },
                colors = ButtonDefaults.buttonColors(
                    containerColor = PS5AccentBlue, contentColor = Color.White
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                Icon(Icons.Default.Check, contentDescription = null, modifier = Modifier.size(16.dp))
                Spacer(Modifier.width(6.dp))
                Text("Run FEXCore JIT conformance test", fontSize = 12.sp, fontWeight = FontWeight.Bold)
            }
            testResult?.let { res ->
                Text(
                    text = res,
                    fontSize = 12.sp,
                    color = if (res.startsWith("PASSED")) Color(0xFF69F0AE) else Color(0xFFFF5252),
                    fontWeight = FontWeight.Bold,
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                    modifier = Modifier.padding(top = 6.dp)
                )
            }
        }
    }
    item {
        fexCoreWrapper?.let { wrapper ->
            var running by remember { mutableStateOf(false) }
            var report by remember { mutableStateOf<String?>(null) }
            val scope = rememberCoroutineScope()

            SettingsSubHeader("Foundation self-test", hint = "evidence only — no fakes")
            Button(
                enabled = !running,
                onClick = {
                    soundManager.playActivationSound()
                    running = true
                    scope.launch(Dispatchers.Default) {
                        val rep = try {
                            wrapper.nativeRunFoundationSelfTest()
                        } catch (e: Exception) {
                            "[FAIL] native: ${e.message}"
                        }
                        report = rep
                        running = false
                    }
                },
                colors = ButtonDefaults.buttonColors(
                    containerColor = DeckTeal, contentColor = Color.Black
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                if (running) {
                    CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
                } else {
                    Icon(Icons.Default.CheckCircle, contentDescription = null, modifier = Modifier.size(16.dp))
                }
                Spacer(Modifier.width(6.dp))
                Text("Run foundation proof pipeline", fontSize = 12.sp, fontWeight = FontWeight.Bold)
            }
            report?.let { rep -> MonoReportBox(rep, passAware = true) }
        }
    }
}

// ---------------------------------------------------------------------------
// 1 — Graphics: engine-coupled controls (real effects)
// ---------------------------------------------------------------------------

private fun LazyListScope.graphicsSection(
    fexCoreWrapper: FexCoreWrapper?,
    soundManager: SoundManager,
    onOpenTurnipManagerClick: () -> Unit
) {
    item {
        val scale = Px5Settings.resScalePct.collectAsState()
        val vsync = Px5Settings.vsyncEnabled.collectAsState()

        SettingsHeader("Graphics")
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text("Resolution scale", fontSize = 14.sp, color = PS5TextPrimary, fontFamily = TitilliumFontFamily, fontWeight = FontWeight.SemiBold)
            Text("${scale.value}%", fontSize = 14.sp, color = PS5AccentGlow, fontFamily = TitilliumFontFamily, fontWeight = FontWeight.Bold)
        }
        Text(
            "Swapchain extent clamp per driver",
            fontSize = 12.sp, color = PS5TextSecondary, fontFamily = TitilliumFontFamily
        )
        Slider(
            value = scale.value.toFloat(),
            onValueChange = { v -> Px5Settings.setResScalePct(v.toInt()) },
            valueRange = 50f..200f,
            steps = 9,
            colors = SliderDefaults.colors(
                thumbColor = PS5AccentBlue, activeTrackColor = PS5AccentGlow
            )
        )
        Spacer(modifier = Modifier.height(8.dp))
        SettingsToggleItem(
            title = "V-Sync / frame pacing",
            subtitle = if (vsync.value) "FIFO present mode (tear-free)"
            else "MAILBOX / IMMEDIATE when available",
            checked = vsync.value,
            onCheckedChange = { v -> Px5Settings.setVsync(v) }
        )
    }
    item {
        var driverSummary by remember { mutableStateOf("") }
        Button(
            onClick = {
                soundManager.playActivationSound()
                driverSummary = fexCoreWrapper?.nativeGetDriverManagerSummary()
                    ?: "engine library unavailable"
            },
            colors = ButtonDefaults.buttonColors(
                containerColor = Color.White.copy(alpha = 0.10f), contentColor = Color.White
            ),
            shape = RoundedCornerShape(12.dp)
        ) {
            Icon(Icons.Default.Refresh, contentDescription = null, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(6.dp))
            Text("Refresh driver state", fontSize = 12.sp)
        }
        if (driverSummary.isNotEmpty()) MonoReportBox(driverSummary)
    }
    item {
        Button(
            onClick = {
                soundManager.playActivationSound()
                onOpenTurnipManagerClick()
            },
            colors = ButtonDefaults.buttonColors(
                containerColor = PS5AccentBlue, contentColor = Color.White
            ),
            shape = RoundedCornerShape(14.dp),
            contentPadding = PaddingValues(horizontal = 20.dp, vertical = 12.dp)
        ) {
            Icon(Icons.Default.Memory, contentDescription = null, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(8.dp))
            Text("Manage GPU drivers (Turnip)", fontWeight = FontWeight.Bold, fontFamily = TitilliumFontFamily)
        }
    }
}

// ---------------------------------------------------------------------------
// 2 — Audio: the two toggles that actually exist
// ---------------------------------------------------------------------------

private fun LazyListScope.audioSection(soundManager: SoundManager) {
    item {
        var soundEffectsEnabled by remember { mutableStateOf(soundManager.isSoundEnabled) }
        var bgMusicEnabled by remember { mutableStateOf(soundManager.isBgMusicEnabled) }
        SettingsHeader("Audio")
        SettingsToggleItem(
            title = "UI sound effects",
            subtitle = "Navigation and activation sounds",
            checked = soundEffectsEnabled,
            onCheckedChange = {
                soundEffectsEnabled = it
                soundManager.isSoundEnabled = it
            }
        )
        SettingsToggleItem(
            title = "Background music",
            subtitle = "Ambient theme music on the home screen",
            checked = bgMusicEnabled,
            onCheckedChange = {
                bgMusicEnabled = it
                soundManager.isBgMusicEnabled = it
            }
        )
        Text(
            text = "In-game audio output lands with the HLE audio path (Phase C).",
            fontSize = 12.sp, color = PS5TextSecondary, fontFamily = TitilliumFontFamily
        )
    }
}

// ---------------------------------------------------------------------------
// 3 — Storage & Games: REAL byte counts from StatFs + real paths
// ---------------------------------------------------------------------------

private fun LazyListScope.storageSection(
    onImportFileClick: () -> Unit,
    onImportFolderClick: () -> Unit,
    onScanGamesClick: () -> Unit
) {
    item {
        val context = androidx.compose.ui.platform.LocalContext.current
        SettingsHeader("Storage")

        val filesDir = context.filesDir
        val extDir = context.getExternalFilesDir(null)
        data class Vol(val label: String, val dir: File?)
        val volumes = listOf(
            Vol("Internal app storage", filesDir),
            Vol("External app storage", extDir)
        )
        volumes.forEach { vol ->
            val dir = vol.dir
            if (dir != null) {
                val stat = StatFs(dir.absolutePath)
                val total = stat.totalBytes
                val free = stat.availableBytes
                SettingsItemText(
                    vol.label,
                    "${formatBytes(free)} free of ${formatBytes(total)} • ${dir.absolutePath}",
                    mono = false
                )
            } else {
                SettingsItemText(vol.label, "not available on this device")
            }
        }
    }
    item {
        SettingsSubHeader("Game library")
        Button(
            onClick = onImportFileClick,
            colors = ButtonDefaults.buttonColors(containerColor = PS5AccentBlue, contentColor = Color.White),
            shape = RoundedCornerShape(12.dp)
        ) {
            Icon(Icons.Default.Description, contentDescription = null, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(6.dp))
            Text("Import file (.pkg / .iso / .elf / .self)", fontSize = 12.sp)
        }
        Spacer(Modifier.height(8.dp))
        Button(
            onClick = onImportFolderClick,
            colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.10f), contentColor = Color.White),
            shape = RoundedCornerShape(12.dp)
        ) {
            Icon(Icons.Default.FolderOpen, contentDescription = null, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(6.dp))
            Text("Import game folder (decrypted dump)", fontSize = 12.sp)
        }
        Spacer(Modifier.height(8.dp))
        Button(
            onClick = onScanGamesClick,
            colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.10f), contentColor = Color.White),
            shape = RoundedCornerShape(12.dp)
        ) {
            Icon(Icons.Default.Search, contentDescription = null, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(6.dp))
            Text("Scan common storage locations", fontSize = 12.sp)
        }
        Text(
            text = "The scan looks for eboot.bin dump folders and .pkg/.iso files in Download, " +
                    "PX5/Games and other readable locations. Nothing is modified or moved.",
            fontSize = 12.sp, color = PS5TextSecondary, fontFamily = TitilliumFontFamily,
            modifier = Modifier.padding(top = 8.dp)
        )
    }
}

// ---------------------------------------------------------------------------
// 4 — GPU Drivers
// ---------------------------------------------------------------------------

private fun LazyListScope.driversSection(onOpenTurnipManagerClick: () -> Unit) {
    item {
        SettingsHeader("GPU drivers")
        SettingsItemText(
            "Active loader",
            "libadrenotools linker-namespace hook; imported Turnip drivers are proven " +
                    "against /proc/self/maps before the engine trusts them."
        )
        Button(
            onClick = onOpenTurnipManagerClick,
            colors = ButtonDefaults.buttonColors(containerColor = PS5AccentBlue, contentColor = Color.White),
            shape = RoundedCornerShape(14.dp),
            contentPadding = PaddingValues(horizontal = 20.dp, vertical = 12.dp)
        ) {
            Icon(Icons.Default.Memory, contentDescription = null, modifier = Modifier.size(18.dp))
            Spacer(Modifier.width(8.dp))
            Text("Open driver manager", fontWeight = FontWeight.Bold, fontFamily = TitilliumFontFamily)
        }
    }
}

// ---------------------------------------------------------------------------
// 5 — Diagnostics: real crash logs (kept from the honest foundation)
// ---------------------------------------------------------------------------

private fun LazyListScope.diagnosticsSection(soundManager: SoundManager) {
    item {
        val context = androidx.compose.ui.platform.LocalContext.current
        var logText by remember { mutableStateOf(com.px5.emulator.PX5Application.getCrashLogs(context)) }

        SettingsHeader("Diagnostics")
        SettingsItemText(
            "Log location",
            (context.getExternalFilesDir(null)?.resolve("logs")?.absolutePath
                ?: context.filesDir.resolve("logs").absolutePath)
        )
        Text(
            text = "Captured crashes and uncaught exceptions:",
            fontSize = 12.sp, color = PS5TextSecondary, fontFamily = TitilliumFontFamily
        )
        Spacer(modifier = Modifier.height(8.dp))
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(220.dp)
                .clip(RoundedCornerShape(12.dp))
                .background(Color.Black.copy(alpha = 0.6f))
                .border(1.dp, Color.White.copy(alpha = 0.15f), RoundedCornerShape(12.dp))
                .padding(12.dp)
        ) {
            LazyColumn(modifier = Modifier.fillMaxSize()) {
                item {
                    Text(
                        text = logText.ifBlank { "(no crash logs recorded)" },
                        fontSize = 11.sp,
                        color = Color(0xFFFF8A80),
                        fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                    )
                }
            }
        }
        Spacer(modifier = Modifier.height(10.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Button(
                onClick = {
                    logText = com.px5.emulator.PX5Application.getCrashLogs(context)
                    soundManager.playNavigationSound()
                },
                colors = ButtonDefaults.buttonColors(containerColor = PS5AccentBlue)
            ) {
                Icon(Icons.Default.Refresh, contentDescription = null, modifier = Modifier.size(16.dp))
                Spacer(Modifier.width(6.dp))
                Text("Refresh", fontSize = 12.sp)
            }
            OutlinedButton(
                onClick = {
                    com.px5.emulator.PX5Application.clearCrashLogs(context)
                    logText = com.px5.emulator.PX5Application.getCrashLogs(context)
                    soundManager.playActivationSound()
                },
                border = androidx.compose.foundation.BorderStroke(1.dp, Color.White.copy(alpha = 0.3f))
            ) {
                Icon(Icons.Default.Delete, contentDescription = null, modifier = Modifier.size(16.dp), tint = Color.White)
                Spacer(Modifier.width(6.dp))
                Text("Clear", fontSize = 12.sp, color = Color.White)
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Shared building blocks
// ---------------------------------------------------------------------------

@Composable
private fun SettingsCategoryTab(
    title: String,
    icon: ImageVector,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(if (isSelected) PS5AccentBlue.copy(alpha = 0.3f) else Color.Transparent)
            .border(
                width = if (isSelected) 1.5.dp else 0.dp,
                color = if (isSelected) PS5AccentGlow else Color.Transparent,
                shape = RoundedCornerShape(14.dp)
            )
            .clickable { onClick() }
            .padding(horizontal = 16.dp, vertical = 14.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                imageVector = icon,
                contentDescription = title,
                tint = if (isSelected) PS5AccentGlow else PS5TextPrimary,
                modifier = Modifier.size(20.dp)
            )
            Spacer(modifier = Modifier.width(12.dp))
            Text(
                text = title,
                fontSize = 14.sp,
                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                color = if (isSelected) PS5TextPrimary else PS5TextSecondary,
                fontFamily = TitilliumFontFamily
            )
        }
    }
}

@Composable
private fun SettingsChip(
    title: String,
    icon: ImageVector,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .clip(RoundedCornerShape(20.dp))
            .background(if (isSelected) PS5AccentBlue.copy(alpha = 0.35f) else Color.White.copy(alpha = 0.06f))
            .border(
                width = if (isSelected) 1.2.dp else 0.dp,
                color = if (isSelected) PS5AccentGlow else Color.Transparent,
                shape = RoundedCornerShape(20.dp)
            )
            .clickable { onClick() }
            .padding(horizontal = 14.dp, vertical = 8.dp)
    ) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            tint = if (isSelected) PS5AccentGlow else PS5TextSecondary,
            modifier = Modifier.size(16.dp)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = title,
            fontSize = 12.sp,
            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
            color = if (isSelected) PS5TextPrimary else PS5TextSecondary,
            fontFamily = TitilliumFontFamily
        )
    }
}

@Composable
private fun SettingsHeader(title: String) {
    Text(
        text = title,
        fontSize = 20.sp,
        fontWeight = FontWeight.Bold,
        color = PS5TextPrimary,
        fontFamily = TitilliumFontFamily,
        modifier = Modifier.padding(bottom = 6.dp)
    )
}

@Composable
private fun SettingsSubHeader(title: String, hint: String = "") {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(
            text = title,
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            color = DeckTeal,
            fontFamily = TitilliumFontFamily
        )
        if (hint.isNotBlank()) {
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = "($hint)",
                fontSize = 11.sp,
                color = PS5TextSecondary,
                fontFamily = TitilliumFontFamily
            )
        }
    }
}

@Composable
private fun SettingsItemText(label: String, value: String, mono: Boolean = false) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp)
    ) {
        Text(
            text = label, fontSize = 14.sp, fontWeight = FontWeight.SemiBold,
            color = PS5TextPrimary, fontFamily = TitilliumFontFamily
        )
        Text(
            text = value,
            fontSize = 12.sp,
            color = PS5TextSecondary,
            fontFamily = if (mono) androidx.compose.ui.text.font.FontFamily.Monospace else TitilliumFontFamily
        )
        Spacer(modifier = Modifier.height(4.dp))
        HorizontalDivider(color = Color.White.copy(alpha = 0.08f))
    }
}

@Composable
private fun SettingsToggleItem(
    title: String,
    subtitle: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = title, fontSize = 14.sp, fontWeight = FontWeight.SemiBold,
                color = PS5TextPrimary, fontFamily = TitilliumFontFamily
            )
            Text(
                text = subtitle, fontSize = 12.sp,
                color = PS5TextSecondary, fontFamily = TitilliumFontFamily
            )
        }
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(
                checkedThumbColor = Color.White,
                checkedTrackColor = PS5AccentBlue
            )
        )
    }
}

@Composable
private fun MonoReportBox(text: String, passAware: Boolean = false) {
    val passed = text.contains("VERDICT: PASS")
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(Color.Black.copy(alpha = 0.55f))
            .border(
                1.dp,
                when {
                    passAware && passed -> Color(0xFF69F0AE).copy(alpha = 0.6f)
                    passAware -> Color(0xFFFF5252).copy(alpha = 0.6f)
                    else -> PS5AccentBlue.copy(alpha = 0.4f)
                },
                RoundedCornerShape(10.dp)
            )
            .padding(10.dp)
    ) {
        Column {
            text.split('\n').forEach { line ->
                val color = when {
                    line.startsWith("[PASS]") || line.startsWith("VERDICT: PASS") -> Color(0xFF69F0AE)
                    line.startsWith("[FAIL]") || line.startsWith("VERDICT: FAIL") -> Color(0xFFFF5252)
                    else -> PS5TextSecondary
                }
                Text(
                    text = line,
                    fontSize = 11.sp,
                    color = color,
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                    modifier = Modifier.padding(vertical = 1.dp)
                )
            }
        }
    }
}
