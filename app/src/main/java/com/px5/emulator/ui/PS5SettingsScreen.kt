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
import androidx.compose.material3.TabRowDefaults
import androidx.compose.material3.TabRowDefaults.tabIndicatorOffset
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
import kotlinx.coroutines.withContext

/**
 * Settings — six fixed tabs mirroring the Eden emulator layout:
 * System / General / Graphics / Input / Audio / Debug.
 *
 * Ground rules for this screen:
 *  * Every control maps to a real stored setting and, where stated, a real
 *    native effect. No decorative switches, no hardcoded status lines.
 *  * Engine internals (counters, self-tests, crash logs, log level) live in
 *    Debug — they are diagnostic tools, not consumer features.
 *  * The section list is data-driven; both orientations use the same panel.
 */
private data class SettingsCategory(val title: String, val icon: ImageVector)

private val CATEGORIES = listOf(
    SettingsCategory("System", Icons.Default.Memory),
    SettingsCategory("General", Icons.Default.Tune),
    SettingsCategory("Graphics", Icons.Default.PlayArrow),
    SettingsCategory("Input", Icons.Default.Gamepad),
    SettingsCategory("Audio", Icons.Default.VolumeUp),
    SettingsCategory("Debug", Icons.Default.BugReport)
)

@OptIn(ExperimentalMaterial3Api::class)
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

    Scaffold(
        containerColor = px5Colors().background,
        topBar = {
            Column {
                TopAppBar(
                    title = { 
                        Text("Settings", fontWeight = FontWeight.Bold, color = px5Colors().text) 
                    },
                    navigationIcon = {
                        IconButton(onClick = {
                            soundManager.playNavigationSound()
                            onBackClick()
                        }) {
                            Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back", tint = px5Colors().text)
                        }
                    },
                    colors = TopAppBarDefaults.topAppBarColors(containerColor = px5Colors().surface)
                )
                ScrollableTabRow(
                    selectedTabIndex = selectedCategory,
                    containerColor = px5Colors().surface,
                    contentColor = px5Colors().accent,
                    edgePadding = 0.dp,
                    indicator = { tabPositions ->
                        TabRowDefaults.SecondaryIndicator(
                            Modifier.tabIndicatorOffset(tabPositions[selectedCategory]),
                            color = px5Colors().accent
                        )
                    }
                ) {
                    CATEGORIES.forEachIndexed { i, cat ->
                        Tab(
                            selected = selectedCategory == i,
                            onClick = {
                                selectedCategory = i
                                soundManager.playNavigationSound()
                            },
                            text = { 
                                Text(
                                    text = cat.title, 
                                    color = if (selectedCategory == i) px5Colors().accent else px5Colors().textSecondary,
                                    fontWeight = if (selectedCategory == i) FontWeight.Bold else FontWeight.Normal
                                ) 
                            }
                        )
                    }
                }
            }
        }
    ) { innerPadding ->
        SettingsPanel(
            modifier = Modifier.padding(innerPadding),
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

@Composable
private fun SettingsPanel(
    modifier: Modifier = Modifier,
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
    LazyColumn(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = 16.dp, vertical = 8.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        when (category) {
            0 -> systemSection(fexCoreStatus, fexCoreWrapper, soundManager)
            1 -> generalSection(soundManager,
                    onImportFileClick, onImportFolderClick, onScanGamesClick)
            2 -> graphicsSection(fexCoreWrapper, soundManager,
                    onOpenTurnipManagerClick)
            3 -> inputSection()
            4 -> audioSection(soundManager)
            5 -> debugSection(fexCoreStatus, fexCoreWrapper, soundManager,
                    onOpenLogsClick)
        }
    }
}

// ---------------------------------------------------------------------------
// 0 — System: CPU translator + translation parameters (real FEXCore config)
// ---------------------------------------------------------------------------

private fun LazyListScope.systemSection(
    fexCoreStatus: String,
    fexCoreWrapper: FexCoreWrapper?,
    soundManager: SoundManager
) {
    item {
        SettingsHeader("System")

        SettingsSubHeader("CPU translator")
        val archSummary = remember { mutableStateOf("") }
        LaunchedEffect(Unit) {
            archSummary.value = try {
                fexCoreWrapper?.nativeGetArchitectureSummary() ?: ""
            } catch (_: Exception) { "" }
        }
        SettingsItemText(
            "Translator",
            archSummary.value.ifBlank {
                "FEXCore x86-64 → ARM64 JIT (libpx5.so)"
            }
        )
        SettingsItemText("Bridge status", fexCoreStatus)
    }
    item {
        // ---- Translation parameters — FEXCore's real config layer -------
        // The preset is DATA (FexCorePresets.kt). Application happens through
        // FexCoreWrapper.nativeApplyEngineConfigOverride → FEXCore::Config
        // before the context exists; verification happens in the Debug
        // counters panel and engine log, never in a toggle's wishful state.
        SettingsSubHeader("Translation parameters")
        val scope = rememberCoroutineScope()
        val presetName by Px5Settings.enginePresetName.collectAsState()
        val overrides by Px5Settings.engineOverrides.collectAsState()
        SettingsSegmented(
            label = "Preset",
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
                // live context would silently ignore every override. The
                // honest path is a REAL restart: shutdown → re-apply every
                // override → re-init — refused with a logged reason if
                // anything fails mid-way.
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
            overrides.entries.joinToString("\n") { "${it.key}=${it.value}" }
                .ifEmpty { "(none — engine defaults apply)" }
        )
    }
}

// ---------------------------------------------------------------------------
// 1 — General: appearance, app facts, game library actions, storage
// ---------------------------------------------------------------------------

private fun LazyListScope.generalSection(
    soundManager: SoundManager,
    onImportFileClick: () -> Unit,
    onImportFolderClick: () -> Unit,
    onScanGamesClick: () -> Unit
) {
    item {
        SettingsHeader("General")
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
        // v1.37 — the "Screen orientation" segmented control is gone from
        // here: the shell orientation is a one-tap flip on the main page
        // (where it belongs), and during a game session the orientation is
        // forced landscape regardless of any preference. A second control
        // in General only invited the portrait-mid-game crash the session
        // gate now prevents.
    }
    item {
        val appInfo = androidx.compose.ui.platform.LocalContext.current
            .packageManager.getPackageInfo(
                androidx.compose.ui.platform.LocalContext.current.packageName, 0
            )
        SettingsItemText("App version", appInfo.versionName ?: "?")
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
    }
    item {
        SettingsSubHeader("Storage")
        val context = androidx.compose.ui.platform.LocalContext.current
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
}

// ---------------------------------------------------------------------------
// 3 — Input: virtual pad controls + physical controller facts
// ---------------------------------------------------------------------------

private fun LazyListScope.inputSection() {
    item {
        SettingsHeader("Input")
        SettingsToggleItem(
            title = "Enable virtual controls",
            subtitle = "Display on-screen controls",
            checked = Px5Settings.showTouchPad.collectAsState().value,
            onCheckedChange = { v -> Px5Settings.setShowTouchPad(v) }
        )
        val opacity = Px5Settings.touchOpacityPct.collectAsState()
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Text("Overlay opacity", fontSize = 14.sp, color = px5Colors().text, fontFamily = TitilliumFontFamily, fontWeight = FontWeight.SemiBold)
            Text("${opacity.value}%", fontSize = 14.sp, color = px5Colors().accentGlow, fontFamily = TitilliumFontFamily, fontWeight = FontWeight.Bold)
        }
        Slider(
            value = opacity.value.toFloat(),
            onValueChange = { v -> Px5Settings.setTouchOpacityPct(v.toInt()) },
            valueRange = 40f..100f,
            steps = 11,
            colors = SliderDefaults.colors(
                thumbColor = PS5AccentBlue, activeTrackColor = px5Colors().accentGlow
            )
        )
    }
}

// ---------------------------------------------------------------------------
// 4 — Audio: the controls that actually exist
// ---------------------------------------------------------------------------

private fun LazyListScope.audioSection(soundManager: SoundManager) {
    item {
        var soundEffectsEnabled by remember { mutableStateOf(soundManager.isSoundEnabled) }
        var bgMusicEnabled by remember { mutableStateOf(soundManager.isBgMusicEnabled) }
        SettingsHeader("Audio")
        SettingsItemText("Audio driver", "AAudio (system)")
        SettingsToggleItem(
            title = "UI sound effects",
            subtitle = "",
            checked = soundEffectsEnabled,
            onCheckedChange = {
                soundEffectsEnabled = it
                soundManager.isSoundEnabled = it
            }
        )
        SettingsToggleItem(
            title = "Background music",
            subtitle = "",
            checked = bgMusicEnabled,
            onCheckedChange = {
                bgMusicEnabled = it
                soundManager.isBgMusicEnabled = it
            }
        )
    }
}

// ---------------------------------------------------------------------------
// 5 — Debug: about device, engine counters, self-tests, logs, crash dumps
// ---------------------------------------------------------------------------

private fun LazyListScope.debugSection(
    fexCoreStatus: String,
    fexCoreWrapper: FexCoreWrapper?,
    soundManager: SoundManager,
    onOpenLogsClick: () -> Unit
) {
    item {
        SettingsHeader("Debug")
        SettingsSubHeader("About device")
        // v1.37 — the Eden-style measured device block (General / CPU / GPU
        // / Memory), identical to the one the engine writes at the top of
        // px5_main.log: system build properties, /proc/cpuinfo core-part
        // topology + feature flags, a REAL Vulkan probe (device name, API
        // version, packed driver version), and MemTotal. First call runs
        // the probe, so it loads off the main thread; while it runs the
        // panel says so instead of showing stale or invented numbers.
        val wrapper = fexCoreWrapper
        var deviceReport by remember { mutableStateOf<String?>(null) }
        LaunchedEffect(wrapper) {
            if (deviceReport != null) return@LaunchedEffect
            deviceReport = withContext(Dispatchers.IO) {
                try {
                    wrapper?.nativeGetHostDeviceInfo()
                        ?: "(no engine on this ABI — device info unavailable)"
                } catch (t: Throwable) {
                    "(device info probe failed: ${t.message})"
                }
            }
        }
        val report = deviceReport
        if (report != null) {
            MonoReportBox(report)
        } else {
            SettingsItemText("Device", "measuring (Vulkan probe running)…")
        }
        SettingsItemText("CPU bridge", fexCoreStatus)
    }
    item {
        SettingsSubHeader("Engine counters")
        // Live engine counters — real numbers read from the running engine
        // (syscalls, SMC faults/invalidations, memory window, thread state).
        var counters by remember { mutableStateOf("") }
        LaunchedEffect(Unit) {
            try { counters = fexCoreWrapper?.nativeGetEngineCounters() ?: "" }
            catch (_: Exception) {}
        }
        if (counters.isNotBlank()) {
            MonoReportBox(counters)
        } else {
        }

    }
    item {
        SettingsSubHeader("Self-tests")
        fexCoreWrapper?.let { wrapper ->
            var testResult by remember { mutableStateOf<String?>(null) }
            val scope = rememberCoroutineScope()
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
                Text("Run CPU conformance test", fontSize = 12.sp, fontWeight = FontWeight.Bold)
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
            Spacer(Modifier.height(10.dp))

            // v1.20 — the CPU-gate discriminator. Same blob, NO fork: if the
            // JIT faults, the app dies and the evidence-first crash handler
            // writes the module-resolved PC into px5_main.log. The button
            // says exactly that — no softened wording.
            var inProcResult by remember { mutableStateOf<String?>(null) }
            Button(
                onClick = {
                    soundManager.playActivationSound()
                    inProcResult = "running in-process (no containment)…"
                    scope.launch(Dispatchers.Default) {
                        val rep = try {
                            wrapper.nativeRunCpuConformanceInProcess()
                        } catch (e: Exception) {
                            "FAILED — ${e.message}"
                        }
                        inProcResult = rep
                    }
                },
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color(0xFFB3591F), contentColor = Color.White
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                Icon(Icons.Default.Warning, contentDescription = null,
                     modifier = Modifier.size(16.dp))
                Spacer(Modifier.width(6.dp))
                Text("Run conformance IN-PROCESS (unsafe — may kill the app)",
                     fontSize = 12.sp, fontWeight = FontWeight.Bold)
            }
                
            Spacer(Modifier.height(10.dp))

            var running by remember { mutableStateOf(false) }
            var report by remember { mutableStateOf<String?>(null) }
            Button(
                enabled = !running,
                onClick = {
                    soundManager.playActivationSound()
                    running = true
                    scope.launch(Dispatchers.Default) {
                        // v1.26: the Settings button follows the auto-run
                        // gate to the in-process path (no fork, no engine
                        // rebuild) — the vc26-proven execution environment.
                        val rep = try {
                            wrapper.nativeRunFoundationSelfTestInProcess()
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
                Text("Run engine self-test (in-process, no fork)", fontSize = 12.sp, fontWeight = FontWeight.Bold)
            }
            report?.let { rep -> MonoReportBox(rep, passAware = true) }
        }
    }
    item {
        SettingsSubHeader("Logs")
        val context = androidx.compose.ui.platform.LocalContext.current
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
        Spacer(modifier = Modifier.height(6.dp))
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
        SettingsItemText(
            "Log location",
            (context.getExternalFilesDir(null)?.resolve("logs")?.absolutePath
                ?: context.filesDir.resolve("logs").absolutePath)
        )
        Spacer(modifier = Modifier.height(8.dp))
        var logText by remember { mutableStateOf(com.px5.emulator.PX5Application.getCrashLogs(context)) }
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
            Text("Game resolution", fontSize = 14.sp, color = px5Colors().text, fontFamily = TitilliumFontFamily, fontWeight = FontWeight.SemiBold)
            Text("${scale.value}%", fontSize = 14.sp, color = px5Colors().accentGlow, fontFamily = TitilliumFontFamily, fontWeight = FontWeight.Bold)
        }
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
            title = "Show FPS",
            subtitle = "Show FPS counter",
            checked = Px5Settings.showFps.collectAsState().value,
            onCheckedChange = { v -> Px5Settings.setShowFps(v) }
        )
        SettingsToggleItem(
            title = "Show frametime",
            subtitle = "Show frame times",
            checked = Px5Settings.showFrametime.collectAsState().value,
            onCheckedChange = { v -> Px5Settings.setShowFrametime(v) }
        )
        SettingsToggleItem(
            title = "V-Sync / frame pacing",
            subtitle = if (vsync.value) "Enable V-Sync"
                       else "MAILBOX / IMMEDIATE when available",
            checked = vsync.value,
            onCheckedChange = { v -> Px5Settings.setVsync(v) }
        )
    }
    item {
        // VRAM usage mode — biases image memory-type selection in
        // VulkanGpuDevice (real effect on where allocations land).
        val vramMode = Px5Settings.vramUsageMode.collectAsState()
        val ctx = androidx.compose.ui.platform.LocalContext.current
        SettingsSegmented(
            label = "VRAM usage",
            options = listOf("Conservative", "Balanced", "Aggressive"),
            selectedIndex = vramMode.value,
            onSelect = { i ->
                Px5Settings.setVramUsageMode(i)
                fexCoreWrapper?.let { w -> Px5Settings.push(w, ctx) }
                soundManager.playNavigationSound()
            }
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
    }
    item {
        SettingsSubHeader("GPU driver")
        SettingsItemText(
            "Loader",
            "adrenotools linker-namespace hook; imported drivers are proven " +
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
        Spacer(Modifier.height(8.dp))
        // Reports the engine's own summary, including the driverVerified=
        // mapping proof. v1.36: AUTO-refreshed — pulled on first composition
        // and re-read on every ON_RESUME (returning from the manager sheet,
        // an import picker, or a game session), so the always-manual
        // "Refresh driver state" button is gone. Eden/Vita3K never ask the
        // user to re-probe by hand; neither does this screen anymore.
        var driverSummary by remember { mutableStateOf("") }
        fun pullDriverSummary() {
            driverSummary = fexCoreWrapper?.nativeGetDriverManagerSummary()
                ?: "engine library unavailable"
        }
        LaunchedEffect(Unit) { pullDriverSummary() }
        val lifecycleOwner = androidx.lifecycle.compose.LocalLifecycleOwner.current
        androidx.compose.runtime.DisposableEffect(lifecycleOwner) {
            val observer = androidx.lifecycle.LifecycleEventObserver { _, event ->
                if (event == androidx.lifecycle.Lifecycle.Event.ON_RESUME) {
                    pullDriverSummary()
                }
            }
            lifecycleOwner.lifecycle.addObserver(observer)
            onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
        }
        if (driverSummary.isNotEmpty()) MonoReportBox(driverSummary)
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
 * Labeled segmented option row — the Eden/Dolphin-style exclusive-choice
 * control. Selection persists through Px5Settings.
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
