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
import com.px5.emulator.core.FexCorePresets
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
    onOpenLogsClick: () -> Unit = {},
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
            .background(px5Colors().background)
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
                        .background(px5Colors().control)
                ) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Back",
                        tint = px5Colors().text
                    )
                }
                Spacer(modifier = Modifier.width(16.dp))
                Text(
                    text = "Settings",
                    fontSize = 26.sp,
                    fontWeight = FontWeight.Bold,
                    color = px5Colors().text,
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
                            onOpenTurnipManagerClick = onOpenTurnipManagerClick,
                            onOpenLogsClick = onOpenLogsClick
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
                            onOpenTurnipManagerClick = onOpenTurnipManagerClick,
                            onOpenLogsClick = onOpenLogsClick
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
    onOpenTurnipManagerClick: () -> Unit,
    onOpenLogsClick: () -> Unit
) {
    Card(
        colors = CardDefaults.cardColors(containerColor = px5Colors().card),
        shape = RoundedCornerShape(18.dp),
        modifier = Modifier
            .fillMaxSize()
            .border(1.dp, px5Colors().hairline, RoundedCornerShape(18.dp))
    ) {
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            when (category) {
                0 -> systemSection(fexCoreStatus, fexCoreWrapper, soundManager)
                1 -> graphicsSection(fexCoreWrapper, soundManager)
                2 -> audioSection(soundManager)
                3 -> storageSection(onImportFileClick, onImportFolderClick, onScanGamesClick)
                4 -> driversSection(fexCoreWrapper, onOpenTurnipManagerClick)
                5 -> diagnosticsSection(soundManager, onOpenLogsClick)
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

        // ---- Appearance (theme + shell orientation) ------------------------
        SettingsSubHeader("Appearance")
        val themeMode by Px5Settings.themeMode.collectAsState()
        SettingsSegmented(
            label = "Theme",
            options = listOf("Dark", "Light", "System"),
            selectedIndex = themeMode,
            onSelect = { i ->
                Px5Settings.setThemeMode(i)
                soundManager.playNavigationSound()
            }
        )
        val orientationMode by Px5Settings.orientationMode.collectAsState()
        SettingsSegmented(
            label = "Screen orientation",
            options = listOf("System", "Landscape", "Portrait"),
            selectedIndex = orientationMode,
            onSelect = { i ->
                Px5Settings.setOrientationMode(i)
                soundManager.playNavigationSound()
            }
        )

        // Engine log level — the real logger gate (Logger::SetMinLevel).
        // "Auto" keeps the legacy verbose-toggle behavior.
        val logLevel by Px5Settings.logLevel.collectAsState()
        SettingsSegmented(
            label = "Engine log level",
            options = listOf("Auto", "None", "Error", "Warn", "Info", "Debug", "Trace"),
            selectedIndex = logLevel + 1,
            onSelect = { i ->
                Px5Settings.setLogLevel(i - 1)
                fexCoreWrapper?.nativeSetLogLevel(i - 1)
                soundManager.playNavigationSound()
            }
        )
        Spacer(modifier = Modifier.height(8.dp))

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
            val scope = rememberCoroutineScope()
            LaunchedEffect(Unit) {
                try {
                    archSummary = wrapper.nativeGetArchitectureSummary()
                } catch (e: Exception) {
                    archSummary = ""
                }
            }

            if (archSummary.isNotBlank()) {
                SettingsSubHeader("Engine status")
                MonoReportBox(archSummary)
            }

            // Live engine counters — real numbers read from the running
            // engine (syscalls, SMC faults/invalidations, memory window,
            // thread state). Zeroes are kept on purpose: they are evidence.
            var counters by remember { mutableStateOf("") }
            LaunchedEffect(Unit) {
                try { counters = wrapper.nativeGetEngineCounters() } catch (_: Exception) {}
            }
            if (counters.isNotBlank()) {
                SettingsSubHeader("Engine counters")
                MonoReportBox(counters)
            }

            Spacer(modifier = Modifier.height(10.dp))
            Button(
                onClick = {
                    soundManager.playActivationSound()
                    testResult = "running in isolated child…"
                    scope.launch(Dispatchers.Default) {
                        val rep = try {
                            wrapper.nativeRunCpuConformanceTest()
                        } catch (e: Exception) {
                            "FAILED — ${e.message}"
                        }
                        testResult = rep
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
                    color = when {
                        res.startsWith("PASSED") -> px5Colors().success
                        res.startsWith("SKIPPED") -> px5Colors().textSecondary
                        else -> px5Colors().danger
                    },
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
                    containerColor = px5Colors().teal, contentColor = Color.Black
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
    item {
        // ---- FEXCore presets — configuration only -----------------------
        // The preset is DATA (FexCorePresets.kt). Application happens through
        // FexCoreWrapper.nativeApplyEngineConfigOverride → FEXCore::Config
        // before the context exists; verification happens in the counters
        // panel and engine log, never in a toggle's wishful checked state.
        SettingsHeader("FEXCore presets")
        val scope = rememberCoroutineScope()
        val presetName by Px5Settings.enginePresetName.collectAsState()
        val overrides by Px5Settings.engineOverrides.collectAsState()
        SettingsSegmented(
            label = "Active preset",
            options = listOf("Safe", "Balanced", "Perf", "Debug"),
            selectedIndex = when (presetName) {
                "Safe (correctness)" -> 0
                "Balanced" -> 1
                "Performance" -> 2
                "Debug / bring-up" -> 3
                else -> 1
            },
            onSelect = { i ->
                val preset = FexCorePresets.BUILT_INS[i]
                val applied = Px5Settings.setEnginePreset(preset.name, preset.overrides)
                // FEX_CONFIG_OPT members are read at context creation, so a
                // live context would silently ignore every override (the
                // 00:41:28 device log: 7× "ignored — engine already
                // initialized"). The honest path is a REAL restart:
                // shutdown → re-apply every override → re-init — refused
                // with a logged reason if anything fails mid-way.
                scope.launch(Dispatchers.Default) {
                    val w = fexCoreWrapper ?: return@launch
                    try {
                        // GetEngineCounters reports exactly "engine: initialized"
                        // or "engine: not initialized".
                        val wasLive = w.nativeGetEngineCounters()
                            ?.contains("engine: initialized") == true
                        if (wasLive) {
                            com.px5.emulator.core.PX5EventLog.event(
                                "fexPreset", "engine_restart_started",
                                "preset=${preset.name}")
                            w.nativeShutdown()
                            applied.forEach { (k, v) ->
                                w.nativeApplyEngineConfigOverride(k, v)
                            }
                            val ok = w.initializeFexCore()
                            com.px5.emulator.core.PX5EventLog.event(
                                "fexPreset", "engine_restart",
                                "preset=${preset.name} keys=${applied.size}",
                                result = ok.toString())
                        } else {
                            applied.forEach { (k, v) ->
                                w.nativeApplyEngineConfigOverride(k, v)
                            }
                            com.px5.emulator.core.PX5EventLog.event(
                                "fexPreset", "overrides_applied_cold",
                                "preset=${preset.name} keys=${applied.size}")
                        }
                    } catch (t: Throwable) {
                        com.px5.emulator.core.PX5EventLog.exception("fexPreset.apply", t)
                    }
                }
                soundManager.playNavigationSound()
            }
        )
        Text(
            text = "Overrides go through FEXCore's real config layer and take effect at engine start. Active overrides:",
            fontSize = 12.sp, color = px5Colors().textSecondary, fontFamily = TitilliumFontFamily
        )
        MonoReportBox(
            overrides.entries.joinToString("\n") { "${it.key}=${it.value}" }
                .ifEmpty { "(no overrides — engine defaults apply)" }
        )
    }
}

// ---------------------------------------------------------------------------
// 1 — Graphics: engine-coupled controls (real effects).
//
// Driver management deliberately lives ONLY in the "GPU Drivers" tab —
// the previous build duplicated the manager entry here and in the driver
// tab, which read as two different controls fighting over one setting.
// ---------------------------------------------------------------------------

private fun LazyListScope.graphicsSection(
    fexCoreWrapper: FexCoreWrapper?,
    soundManager: SoundManager
) {
    item {
        val scale = Px5Settings.resScalePct.collectAsState()
        val vsync = Px5Settings.vsyncEnabled.collectAsState()

        SettingsHeader("Graphics")
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text("Resolution scale", fontSize = 14.sp, color = px5Colors().text, fontFamily = TitilliumFontFamily, fontWeight = FontWeight.SemiBold)
            Text("${scale.value}%", fontSize = 14.sp, color = px5Colors().accentGlow, fontFamily = TitilliumFontFamily, fontWeight = FontWeight.Bold)
        }
        Text(
            "Swapchain extent clamp per driver",
            fontSize = 12.sp, color = px5Colors().textSecondary, fontFamily = TitilliumFontFamily
        )
        Slider(
            value = scale.value.toFloat(),
            onValueChange = { v -> Px5Settings.setResScalePct(v.toInt()) },
            valueRange = 50f..200f,
            steps = 9,
            colors = SliderDefaults.colors(
                thumbColor = PS5AccentBlue, activeTrackColor = px5Colors().accentGlow
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

        // Present mode — explicit selection, validated against the device's
        // supported modes at swapchain build (unsupported choices fall back
        // loudly in the log; the render HUD shows the real mode in use).
        val presentMode by Px5Settings.presentMode.collectAsState()
        SettingsSegmented(
            label = "Present mode",
            options = listOf("Auto", "FIFO", "FIFO_RELAXED", "MAILBOX", "IMMEDIATE", "LATEST_READY"),
            selectedIndex = presentMode,
            onSelect = { i ->
                Px5Settings.setPresentMode(i)
                fexCoreWrapper?.nativeSetPresentMode(i)
                soundManager.playNavigationSound()
            }
        )
        Text(
            text = "Modes the device does not report fall back automatically — the HUD shows the real mode.",
            fontSize = 12.sp, color = px5Colors().textSecondary, fontFamily = TitilliumFontFamily
        )
        Text(
            text = "GPU driver selection and import live in the GPU Drivers tab.",
            fontSize = 12.sp, color = px5Colors().textSecondary, fontFamily = TitilliumFontFamily,
            modifier = Modifier.padding(top = 6.dp)
        )
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
            fontSize = 12.sp, color = px5Colors().textSecondary, fontFamily = TitilliumFontFamily
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
            colors = ButtonDefaults.buttonColors(containerColor = px5Colors().control, contentColor = px5Colors().text),
            shape = RoundedCornerShape(12.dp)
        ) {
            Icon(Icons.Default.FolderOpen, contentDescription = null, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(6.dp))
            Text("Import game folder (decrypted dump)", fontSize = 12.sp)
        }
        Spacer(Modifier.height(8.dp))
        Button(
            onClick = onScanGamesClick,
            colors = ButtonDefaults.buttonColors(containerColor = px5Colors().control, contentColor = px5Colors().text),
            shape = RoundedCornerShape(12.dp)
        ) {
            Icon(Icons.Default.Search, contentDescription = null, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(6.dp))
            Text("Scan common storage locations", fontSize = 12.sp)
        }
        Text(
            text = "The scan looks for eboot.bin dump folders and .pkg/.iso files in Download, " +
                    "PX5/Games and other readable locations. Nothing is modified or moved.",
            fontSize = 12.sp, color = px5Colors().textSecondary, fontFamily = TitilliumFontFamily,
            modifier = Modifier.padding(top = 8.dp)
        )
    }
}

// ---------------------------------------------------------------------------
// 4 — GPU Drivers
// ---------------------------------------------------------------------------

private fun LazyListScope.driversSection(
    fexCoreWrapper: FexCoreWrapper?,
    onOpenTurnipManagerClick: () -> Unit
) {
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
    item {
        // The single "refresh driver state" control (was duplicated in the
        // Graphics tab). Reports the engine's own summary, including the
        // driverVerified= mapping proof.
        var driverSummary by remember { mutableStateOf("") }
        Button(
            onClick = {
                driverSummary = fexCoreWrapper?.nativeGetDriverManagerSummary()
                    ?: "engine library unavailable"
            },
            colors = ButtonDefaults.buttonColors(
                containerColor = px5Colors().control, contentColor = px5Colors().text
            ),
            shape = RoundedCornerShape(12.dp)
        ) {
            Icon(Icons.Default.Refresh, contentDescription = null, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(6.dp))
            Text("Refresh driver state", fontSize = 12.sp)
        }
        if (driverSummary.isNotEmpty()) MonoReportBox(driverSummary)
    }
}

// ---------------------------------------------------------------------------
// 5 — Diagnostics: real crash logs (kept from the honest foundation)
// ---------------------------------------------------------------------------

private fun LazyListScope.diagnosticsSection(
    soundManager: SoundManager,
    onOpenLogsClick: () -> Unit
) {
    item {
        val context = androidx.compose.ui.platform.LocalContext.current
        var logText by remember { mutableStateOf(com.px5.emulator.PX5Application.getCrashLogs(context)) }
        val verbose = Px5Settings.verboseLogging.collectAsState()

        SettingsHeader("Diagnostics")

        // Live viewer over the three REAL sinks: app diagnostic log,
        // native rotating engine log (px5_main.log), crash dumps.
        Button(
            onClick = {
                soundManager.playNavigationSound()
                onOpenLogsClick()
            },
            colors = ButtonDefaults.buttonColors(containerColor = PS5AccentBlue, contentColor = Color.White),
            shape = RoundedCornerShape(12.dp)
        ) {
            Icon(Icons.Default.Article, contentDescription = null, modifier = Modifier.size(16.dp))
            Spacer(Modifier.width(6.dp))
            Text("Open live log viewer", fontSize = 12.sp, fontWeight = FontWeight.Bold)
        }
        Spacer(modifier = Modifier.height(6.dp))
        SettingsToggleItem(
            title = "Verbose engine logging",
            subtitle = "Higher detail in px5_main.log (log level)",
            checked = verbose.value,
            onCheckedChange = { v -> Px5Settings.setVerbose(v) }
        )

        SettingsItemText(
            "Log location",
            (context.getExternalFilesDir(null)?.resolve("logs")?.absolutePath
                ?: context.filesDir.resolve("logs").absolutePath)
        )
        Text(
            text = "Captured crashes and uncaught exceptions:",
            fontSize = 12.sp, color = px5Colors().textSecondary, fontFamily = TitilliumFontFamily
        )
        Spacer(modifier = Modifier.height(8.dp))
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(220.dp)
                .clip(RoundedCornerShape(12.dp))
                .background(Color.Black.copy(alpha = 0.6f))
                .border(1.dp, px5Colors().hairline, RoundedCornerShape(12.dp))
                .padding(12.dp)
        ) {
            LazyColumn(modifier = Modifier.fillMaxSize()) {
                item {
                    Text(
                        text = logText.ifBlank { "(no crash logs recorded)" },
                        fontSize = 11.sp,
                        color = px5Colors().danger,
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
                border = androidx.compose.foundation.BorderStroke(1.dp, px5Colors().hairline)
            ) {
                Icon(Icons.Default.Delete, contentDescription = null, modifier = Modifier.size(16.dp), tint = px5Colors().text)
                Spacer(Modifier.width(6.dp))
                Text("Clear", fontSize = 12.sp, color = px5Colors().text)
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
                color = if (isSelected) px5Colors().accentGlow else Color.Transparent,
                shape = RoundedCornerShape(14.dp)
            )
            .clickable { onClick() }
            .padding(horizontal = 16.dp, vertical = 14.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                imageVector = icon,
                contentDescription = title,
                tint = if (isSelected) px5Colors().accentGlow else px5Colors().text,
                modifier = Modifier.size(20.dp)
            )
            Spacer(modifier = Modifier.width(12.dp))
            Text(
                text = title,
                fontSize = 14.sp,
                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
                color = if (isSelected) px5Colors().text else px5Colors().textSecondary,
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
            .background(if (isSelected) PS5AccentBlue.copy(alpha = 0.35f) else px5Colors().card)
            .border(
                width = if (isSelected) 1.2.dp else 0.dp,
                color = if (isSelected) px5Colors().accentGlow else Color.Transparent,
                shape = RoundedCornerShape(20.dp)
            )
            .clickable { onClick() }
            .padding(horizontal = 14.dp, vertical = 8.dp)
    ) {
        Icon(
            imageVector = icon,
            contentDescription = null,
            tint = if (isSelected) px5Colors().accentGlow else px5Colors().textSecondary,
            modifier = Modifier.size(16.dp)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = title,
            fontSize = 12.sp,
            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
            color = if (isSelected) px5Colors().text else px5Colors().textSecondary,
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
        color = px5Colors().text,
        fontFamily = TitilliumFontFamily,
        modifier = Modifier.padding(bottom = 6.dp)
    )
}

/**
 * Labeled segmented option row — the Dolphin/Eden-style exclusive-choice
 * control. Used for Theme and Screen orientation; selection persists via
 * Px5Settings.
 */
@Composable
private fun SettingsSegmented(
    label: String,
    options: List<String>,
    selectedIndex: Int,
    onSelect: (Int) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp)
    ) {
        Text(
            text = label,
            fontSize = 14.sp,
            fontWeight = FontWeight.SemiBold,
            color = px5Colors().text,
            fontFamily = TitilliumFontFamily
        )
        Spacer(modifier = Modifier.height(6.dp))
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(12.dp))
                .background(px5Colors().control)
                .border(1.dp, px5Colors().hairline, RoundedCornerShape(12.dp))
        ) {
            options.forEachIndexed { i, opt ->
                val selected = i == selectedIndex
                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .weight(1f)
                        .clip(RoundedCornerShape(12.dp))
                        .background(
                            if (selected) PS5AccentBlue.copy(alpha = 0.55f)
                            else Color.Transparent
                        )
                        .clickable { onSelect(i) }
                        .padding(vertical = 10.dp)
                ) {
                    Text(
                        text = opt,
                        fontSize = 12.sp,
                        fontWeight = if (selected) FontWeight.Bold else FontWeight.Medium,
                        color = if (selected) Color.White else px5Colors().textSecondary,
                        fontFamily = TitilliumFontFamily,
                        maxLines = 1
                    )
                }
            }
        }
    }
}

@Composable
private fun SettingsSubHeader(title: String, hint: String = "") {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Text(
            text = title,
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            color = px5Colors().teal,
            fontFamily = TitilliumFontFamily
        )
        if (hint.isNotBlank()) {
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = "($hint)",
                fontSize = 11.sp,
                color = px5Colors().textSecondary,
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
            color = px5Colors().text, fontFamily = TitilliumFontFamily
        )
        Text(
            text = value,
            fontSize = 12.sp,
            color = px5Colors().textSecondary,
            fontFamily = if (mono) androidx.compose.ui.text.font.FontFamily.Monospace else TitilliumFontFamily
        )
        Spacer(modifier = Modifier.height(4.dp))
        HorizontalDivider(color = px5Colors().control)
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
                color = px5Colors().text, fontFamily = TitilliumFontFamily
            )
            Text(
                text = subtitle, fontSize = 12.sp,
                color = px5Colors().textSecondary, fontFamily = TitilliumFontFamily
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
                    passAware && passed -> px5Colors().success.copy(alpha = 0.6f)
                    passAware -> px5Colors().danger.copy(alpha = 0.6f)
                    else -> PS5AccentBlue.copy(alpha = 0.4f)
                },
                RoundedCornerShape(10.dp)
            )
            .padding(10.dp)
    ) {
        Column {
            text.split('\n').forEach { line ->
                val color = when {
                    line.startsWith("[PASS]") || line.startsWith("VERDICT: PASS") -> px5Colors().success
                    line.startsWith("[FAIL]") || line.startsWith("VERDICT: FAIL") -> px5Colors().danger
                    else -> px5Colors().textSecondary
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
