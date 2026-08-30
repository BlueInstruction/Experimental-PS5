package com.px5.emulator.ui

import android.view.SurfaceHolder
import android.view.SurfaceView
import android.content.Context
import android.content.pm.ActivityInfo
import android.provider.DocumentsContract
import com.px5.emulator.PhysicalControllerBridge
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.px5.emulator.core.FexCoreWrapper
import com.px5.emulator.core.Px5Settings
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.roundToInt

// ---------------------------------------------------------------------------
// EmuScreen — REAL emulation-stage shell, V2.
//
// Layout contract (matching the reference app the user approved):
//   * the stage FORCES landscape and hides the system bars while open,
//   * the game surface fills the whole screen — no letterbox frame,
//   * boot diagnostics collapse behind one chip (no text walls over the
//     game), the single status line stays honest and live,
//   * the V2 DualSense overlay floats over the game (PS5VirtualPad.kt),
//     with a drag-to-edit layer whose layout persists.
//
// Re-entry safety: the boot diagnostics (GPU proof / GNM / loader
// self-tests) run ONCE per process. Re-running them on every entry is
// the strongest suspect for the "second launch kills the app" report
// (same-process engine re-entry after a renderer attach/detach cycle);
// they already produced their evidence, so re-entry only re-attaches
// the surface and resumes the HUD poll.
// ---------------------------------------------------------------------------
@Composable
fun EmuScreen(
    path: String,
    fexCoreStatus: String,
    fexCoreWrapper: FexCoreWrapper?,
    gameViewModel: com.px5.emulator.GameViewModel? = null,
    onBackClick: () -> Unit
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val activity = context as? android.app.Activity

    var renderStats by remember { mutableStateOf("renderer idle") }
    var fpsText by remember { mutableStateOf("") }
    var frametimeText by remember { mutableStateOf("") }
    var gpuProof by remember { mutableStateOf<String?>(null) }
    var gnmSelfTest by remember { mutableStateOf<String?>(null) }
    var loaderSelfTest by remember { mutableStateOf<String?>(null) }
    var inputSummary by remember { mutableStateOf("input: -") }
    var loadResult by remember { mutableStateOf<String?>(null) }
    var diagOpen by remember { mutableStateOf(false) }
    var padEditing by remember { mutableStateOf(false) }
    // v1.16.1 behavior kept: a crash report surfaces ITSELF — the
    // diagnostics panel auto-opens so evidence is never hidden.
    LaunchedEffect(gpuProof, gnmSelfTest, loaderSelfTest) {
        if (listOfNotNull(gpuProof, gnmSelfTest, loaderSelfTest)
                .any { it.contains("CRASHED") }) diagOpen = true
    }
    var padLayout by remember {
        mutableStateOf(PadLayout.decode(Px5Settings.padLayoutJson.value))
    }

    // ---- stage shell: force landscape + immersive, restore on exit --------
    DisposableEffect(Unit) {
        activity?.requestedOrientation =
            ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        val controller = activity?.window?.let { win ->
            WindowCompat.getInsetsController(win, win.decorView)
        }
        controller?.systemBarsBehavior =
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        controller?.hide(WindowInsetsCompat.Type.systemBars())
        onDispose {
            activity?.let { Px5Settings.applyOrientation(it) }
            controller?.show(WindowInsetsCompat.Type.systemBars())
        }
    }

    // Resolve the library entry behind this path and credit session time.
    val games = gameViewModel?.allGames?.collectAsStateWithLifecycle()?.value ?: emptyList()
    val game = remember(games, path) { games.firstOrNull { it.path == path } }

    LaunchedEffect(game?.id) {
        game?.let { gameViewModel?.touchPlayed(it.id) }
    }
    DisposableEffect(game?.id) {
        val startedAt = System.currentTimeMillis()
        onDispose {
            val seconds = (System.currentTimeMillis() - startedAt) / 1000
            game?.let { gameViewModel?.addPlayTime(it.id, seconds) }
            com.px5.emulator.core.PX5EventLog.event(
                "gameBoot", "game_exit", "path=$path seconds=$seconds")
            try {
                fexCoreWrapper?.nativeLogEvent(
                    "gameBoot", "game_exit seconds=$seconds path=$path")
            } catch (_: Throwable) {}
        }
    }

    // Physical gamepad pass-through lives only while this screen exists.
    DisposableEffect(fexCoreWrapper) {
        PhysicalControllerBridge.wrapper = fexCoreWrapper
        PhysicalControllerBridge.enabled = true
        onDispose {
            PhysicalControllerBridge.enabled = false
            PhysicalControllerBridge.reset()
        }
    }

    val isStubAbi = fexCoreWrapper?.nativeGetArchitectureSummary()
        ?.contains("UI-smoke ABI") == true

    val showPad by Px5Settings.showTouchPad.collectAsState()
    val padOpacity by Px5Settings.touchOpacityPct.collectAsState()
    val padScale by Px5Settings.padScalePct.collectAsState()
    val showFps by Px5Settings.showFps.collectAsState()
    val showFrametime by Px5Settings.showFrametime.collectAsState()

    // ---- boot diagnostics: ONCE per process (re-entry hardening) ----------
    LaunchedEffect(Unit) {
        com.px5.emulator.core.PX5EventLog.event("gameBoot", "screen_entered",
                "path=$path stub=${isStubAbi} diagRan=${EmuScreenBoot.bootDiagnosticsDone}")
        try {
            val ebootStatus = probeEbootStatus(path, context)
            fexCoreWrapper?.nativeLogEvent("gameBoot",
                    "screen_entered game=${game?.name ?: "?"} titleId=${game?.titleId ?: "?"} " +
                    "format=${game?.format ?: "?"} size=${game?.sizeBytes ?: 0L} " +
                    "eboot=$ebootStatus")
        } catch (_: Throwable) {}
        if (EmuScreenBoot.bootDiagnosticsDone) {
            // Second entry in this process: skip the isolated probes. They
            // already reported; the unified log keeps their evidence. This
            // avoids re-driving the engine through proof child + renderer
            // attach cycles, the prime suspect for the second-launch abort.
            gpuProof = "SKIP | already ran this process (see Diagnostics > Logs)"
            return@LaunchedEffect
        }
        EmuScreenBoot.bootDiagnosticsDone = true
        if (!isStubAbi) {
            launch(Dispatchers.Default) {
                gpuProof = try {
                    fexCoreWrapper?.nativeRunGpuProof() ?: "wrapper missing"
                } catch (t: Throwable) { "FAIL | ${t.message}" }
                com.px5.emulator.core.PX5EventLog.event("gameBoot", "gpu_proof",
                        "result=${gpuProof?.take(120)}")
            }
        }
        launch(Dispatchers.Default) {
            gnmSelfTest = try {
                fexCoreWrapper?.nativeRunGnmSelfTest() ?: "wrapper missing"
            } catch (t: Throwable) { "FAIL | ${t.message}" }
            com.px5.emulator.core.PX5EventLog.event("gameBoot", "gnm_selftest",
                    "result=${gnmSelfTest?.lineSequence()?.firstOrNull() ?: "?"}")
        }
        launch(Dispatchers.Default) {
            loaderSelfTest = try {
                fexCoreWrapper?.nativeRunLoaderSelfTest() ?: "wrapper missing"
            } catch (t: Throwable) { "FAIL | ${t.message}" }
            com.px5.emulator.core.PX5EventLog.event("gameBoot", "loader_selftest",
                    "result=${loaderSelfTest?.lineSequence()?.firstOrNull() ?: "?"}")
        }
    }

    // ---- live HUD polling (unchanged semantics) ----------------------------
    var lastFrames by remember { mutableLongStateOf(0L) }
    var lastPollMs by remember { mutableLongStateOf(0L) }
    LaunchedEffect(Unit) {
        while (true) {
            try {
                renderStats = fexCoreWrapper?.nativeGetRenderStats()
                    ?: "no engine"
                val now = System.currentTimeMillis()
                val m = Regex("frames=(\\d+)").find(renderStats)
                val frames = m?.groupValues?.get(1)?.toLongOrNull() ?: -1L
                if (frames >= 0 && lastPollMs > 0L && frames >= lastFrames) {
                    val dFrames = frames - lastFrames
                    val dMs = (now - lastPollMs).coerceAtLeast(1L)
                    if (dFrames > 0) {
                        fpsText = "FPS: %.1f".format(dFrames * 1000.0 / dMs)
                        frametimeText = "frametime: %.1f ms".format(dMs.toDouble() / dFrames)
                    }
                }
                if (frames >= 0) { lastFrames = frames; lastPollMs = now }
                if (!isStubAbi) {
                    inputSummary = fexCoreWrapper?.nativeGetInputSummary()
                        ?: "-"
                }
            } catch (_: Throwable) {}
            delay(500)
        }
    }

    // ---- the stage ----------------------------------------------------------
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
    ) {
        // Fullscreen game surface — the engine owns the whole stage.
        AndroidView(
            factory = { ctx ->
                SurfaceView(ctx).apply {
                    holder.addCallback(object : SurfaceHolder.Callback2 {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            try {
                                val attached =
                                    fexCoreWrapper?.nativeAttachRenderSurface(holder.surface) == true
                                if (attached) {
                                    fexCoreWrapper?.nativeStartRenderer()
                                }
                            } catch (_: Throwable) {}
                        }
                        override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, hh: Int) {}
                        override fun surfaceDestroyed(h: SurfaceHolder) {
                            try {
                                fexCoreWrapper?.nativeStopRenderer()
                                fexCoreWrapper?.nativeDetachRenderSurface()
                            } catch (_: Throwable) {}
                        }
                        override fun surfaceRedrawNeeded(h: SurfaceHolder) {}
                        override fun surfaceRedrawNeededAsync(h: SurfaceHolder, r: Runnable) { r.run() }
                    })
                    setZOrderMediaOverlay(false)
                }
            },
            modifier = Modifier.matchParentSize()
        )
        // ---- overlay layer 1: the V2 DualSense pad -------------------------
        if (showPad) {
            DualSensePadV2(
                enabled = !isStubAbi,
                opacity01 = padOpacity / 100f,
                scale01 = padScale / 100f,
                editing = padEditing,
                layout = padLayout,
                onLayoutChange = { updated ->
                    padLayout = updated
                    Px5Settings.setPadLayoutJson(PadLayout.encode(updated))
                },
                onButton = { bit, down ->
                    fexCoreWrapper?.nativeSetButtonState(bit, down)
                },
                onLeftStick = { x, y -> fexCoreWrapper?.nativeSetLeftStick(x, y) },
                onRightStick = { x, y -> fexCoreWrapper?.nativeSetRightStick(x, y) },
                onTriggers = { l2, r2 -> fexCoreWrapper?.nativeSetTriggers(l2, r2) },
                onTouchpad = { down -> fexCoreWrapper?.nativeSetTouchpad(down) }
            )
        }

        // ---- overlay layer 2: top bar (back / title / chips) ----------------
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(Color.Black.copy(alpha = 0.35f))
                .padding(horizontal = 10.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            IconButton(
                onClick = {
                    padEditing = false
                    onBackClick()
                },
                modifier = Modifier
                    .size(36.dp)
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.14f))
            ) {
                Icon(
                    Icons.AutoMirrored.Filled.ArrowBack,
                    contentDescription = "Exit",
                    tint = Color.White,
                    modifier = Modifier.size(18.dp)
                )
            }
            Spacer(Modifier.width(8.dp))
            Column {
                Text(
                    text = game?.name ?: path.substringAfterLast('/'),
                    color = Color.White, fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = TitilliumFontFamily, maxLines = 1
                )
                Text(
                    text = game?.titleId?.let { "$it · " }.let { t ->
                        (t ?: "") + "frames=" + (Regex("frames=(\\d+)").find(renderStats)?.groupValues?.get(1) ?: "0")
                    },
                    color = Color(0xFF2E8CFF), fontSize = 10.sp,
                    fontFamily = TitilliumFontFamily, maxLines = 1
                )
            }
            Spacer(Modifier.weight(1f))
            if (showFps && fpsText.isNotEmpty()) {
                StatusChip(fpsText, Color(0xFF69F0AE))
                Spacer(Modifier.width(6.dp))
            }
            if (showFrametime && frametimeText.isNotEmpty()) {
                StatusChip(frametimeText, Color(0xFF7DD3FC))
                Spacer(Modifier.width(6.dp))
            }
            IconButton(
                onClick = { Px5Settings.setShowTouchPad(!showPad) },
                modifier = Modifier
                    .size(34.dp)
                    .clip(CircleShape)
                    .background(
                        if (showPad) Color(0xFF0070D1).copy(alpha = 0.55f)
                        else Color.White.copy(alpha = 0.14f)
                    )
            ) {
                Icon(
                    Icons.Default.SportsEsports, contentDescription = "Toggle pad",
                    tint = Color.White, modifier = Modifier.size(17.dp)
                )
            }
            Spacer(Modifier.width(6.dp))
            IconButton(
                onClick = { if (showPad) padEditing = !padEditing },
                modifier = Modifier
                    .size(34.dp)
                    .clip(CircleShape)
                    .background(
                        if (padEditing) Color(0xFFE2C74B).copy(alpha = 0.55f)
                        else Color.White.copy(alpha = 0.14f)
                    )
            ) {
                Icon(
                    Icons.Default.Edit, contentDescription = "Edit pad layout",
                    tint = Color.White, modifier = Modifier.size(15.dp)
                )
            }
            Spacer(Modifier.width(6.dp))
            IconButton(
                onClick = { diagOpen = !diagOpen },
                modifier = Modifier
                    .size(34.dp)
                    .clip(CircleShape)
                    .background(
                        if (diagOpen) Color(0xFF2E8CFF).copy(alpha = 0.55f)
                        else Color.White.copy(alpha = 0.14f)
                    )
            ) {
                Icon(
                    Icons.Default.Info, contentDescription = "Boot diagnostics",
                    tint = Color.White, modifier = Modifier.size(17.dp)
                )
            }
        }

        // ---- overlay layer 3: live status line (always visible, honest) ----
        val proofOk = gpuProof == null ||
            (gpuProof?.lineSequence()?.firstOrNull() ?: "").contains("PASS |") ||
            (gpuProof?.startsWith("SKIP") == true)
        Row(
            modifier = Modifier
                .align(Alignment.BottomStart)
                .padding(start = 10.dp, bottom = 6.dp, end = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(8.dp)
                    .clip(CircleShape)
                    .background(
                        when {
                            gpuProof == null -> Color(0xFFE2C74B)
                            gpuProof?.startsWith("SKIP") == true -> Color(0xFF9BA7BC)
                            proofOk -> Color(0xFF69F0AE)
                            else -> Color(0xFFFF6B6B)
                        }
                    )
            )
            Spacer(Modifier.width(6.dp))
            Text(
                text = renderStats.lineSequence().firstOrNull() ?: renderStats,
                fontSize = 10.sp,
                color = Color.White.copy(alpha = 0.75f),
                fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                maxLines = 1
            )
        }

        // ---- overlay layer 4: boot diagnostics panel (collapsed by default)
        if (diagOpen) {
            Column(
                modifier = Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 52.dp, start = 10.dp, end = 10.dp)
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(Color.Black.copy(alpha = 0.72f))
                    .border(1.dp, Color.White.copy(alpha = 0.15f), RoundedCornerShape(12.dp))
                    .padding(horizontal = 12.dp, vertical = 8.dp)
            ) {
                Text(
                    "GPU device: " +
                        (Regex("GPU device: ([^|]+)").find(renderStats)
                            ?.groupValues?.get(1)?.trim() ?: "?") +
                        "  |  " + inputSummary,
                    fontSize = 10.sp,
                    color = Color(0xFF69F0AE),
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
                gpuProof?.let { p ->
                    val firstLine = p.lineSequence().firstOrNull() ?: p
                    Text(
                        text = "GPU self-test: $firstLine" +
                               (if (firstLine.contains("CRASHED"))
                                   "\n  → driver faulted in the isolated proof child — app survived; dump in px5_main.log"
                               else ""),
                        fontSize = 10.sp,
                        color = if (firstLine.contains("PASS")) Color(0xFF69F0AE)
                                else if (firstLine.startsWith("SKIP")) Color(0xFF9BA7BC)
                                else Color(0xFFFF8A65),
                        fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                    )
                }
                gnmSelfTest?.let { g ->
                    Text(
                        text = "GNM PM4: ${g.lineSequence().firstOrNull() ?: g}",
                        fontSize = 10.sp,
                        color = if ((g.lineSequence().firstOrNull() ?: "").contains("PASS"))
                            Color(0xFF7DD3FC) else Color(0xFFFF8A65),
                        fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                    )
                }
                loaderSelfTest?.let { s ->
                    Text(
                        text = "SELF loader: ${s.lineSequence().firstOrNull() ?: s}",
                        fontSize = 10.sp,
                        color = if ((s.lineSequence().firstOrNull() ?: "").contains("PASS"))
                            Color(0xFF7DD3FC) else Color(0xFFFF8A65),
                        fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                    )
                }
                val gpuProofOk = gpuProof == null ||
                    gpuProof.orEmpty().contains("PASS |")
                if (!isStubAbi && !gpuProofOk && gpuProof?.startsWith("SKIP") != true) {
                    Text(
                        text = "MILESTONE: GPU proof not passing — CPU/GPU work " +
                               "continues before game testing; a load attempt is " +
                               "a diagnostic only",
                        fontSize = 10.sp,
                        color = Color(0xFFE2C74B),
                        fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                    )
                }
                Text(
                    text = "CPU bridge: $fexCoreStatus" +
                           (if (isStubAbi) "  •  UI-smoke ABI (engine=arm64-only)" else ""),
                    fontSize = 10.sp,
                    color = Color(0xFF9BA7BC)
                )
                if (!isStubAbi && fexCoreWrapper != null) {
                    Button(
                        onClick = {
                            scope.launch(Dispatchers.IO) {
                                val target = run {
                                    val f = java.io.File(path)
                                    if (f.isDirectory) {
                                        // v1.19: dump tools land the executable below
                                        // the game root (decrypted/eboot.bin). The
                                        // direct-children-only scan is what produced
                                        // the empty target= / LOAD FAILED in the
                                        // 2026-08-30 v1.18 session.
                                        com.px5.emulator.EbootLocator.find(f)?.also {
                                            com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                                    "exec_target",
                                                    "rel=${it.relPath} depth=${it.depth} dirs=${it.dirsVisited}")
                                        }?.file?.absolutePath ?: ""
                                    } else path
                                }
                                com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                        "exec_load_started", "target=$target")
                                fexCoreWrapper?.nativeLogEvent("gameBoot",
                                        "exec_load_started target=$target")
                                loadResult = if (target.isBlank()) {
                                    com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                            "exec_load_failed",
                                            "reason=no eboot.bin in folder (tree searched to depth " +
                                            "${com.px5.emulator.EbootLocator.MAX_DEPTH})")
                                    "LOAD FAILED: no eboot.bin in folder " +
                                            "(folder tree searched to depth " +
                                            "${com.px5.emulator.EbootLocator.MAX_DEPTH})"
                                } else {
                                    try {
                                        val probe = fexCoreWrapper.nativeLoadExecutableIsolated(target)
                                        com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                                "exec_probe",
                                                "target=${target.substringAfterLast('/')}",
                                                result = probe.lineSequence().firstOrNull()?.take(120) ?: "?")
                                        fexCoreWrapper.nativeLogEvent("gameBoot",
                                                "exec_probe ${probe.lineSequence().firstOrNull()?.take(160) ?: probe.take(160)} " +
                                                "target=${target.substringAfterLast('/')}")
                                        if (probe.startsWith("LOAD OK")) {
                                            val ok = fexCoreWrapper.nativeLoadExecutable(target)
                                            com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                                    "exec_load", "target=${target.substringAfterLast('/')}",
                                                    result = ok.toString())
                                            fexCoreWrapper.nativeLogEvent("gameBoot",
                                                    "exec_load result=$ok target=${target.substringAfterLast('/')}")
                                            if (ok) {
                                                val exec = fexCoreWrapper.nativeRunExecutionProbe(target, 8000)
                                                com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                                        "exec_probe_run",
                                                        "target=${target.substringAfterLast('/')}",
                                                        result = exec.lineSequence().firstOrNull()?.take(120) ?: "?")
                                                fexCoreWrapper.nativeLogEvent("gameBoot",
                                                        "exec_probe_run ${exec.lineSequence().firstOrNull()?.take(160) ?: exec.take(160)}")
                                                val prefix = if (exec.contains("CRASHED"))
                                                    "EXECUTION CONTAINED — app survived (dump on disk):\n"
                                                else ""
                                                "LOADED: $target mapped into guest window\n$prefix$exec"
                                            } else {
                                                "LOAD FAILED: loader rejected $target (see logcat)"
                                            }
                                        } else {
                                            com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                                    "exec_probe_blocked",
                                                    "report=${probe.take(200)}")
                                            "ISOLATED LOAD STOPPED THE CRASH — app survived:\n$probe"
                                        }
                                    } catch (t: Throwable) {
                                        com.px5.emulator.core.PX5EventLog.exception("gameBoot.execLoad", t)
                                        fexCoreWrapper.nativeLogEvent("gameBoot",
                                                "exec_load EXCEPTION ${t.javaClass.simpleName}: ${t.message}")
                                        "LOAD FAILED: ${t.message}"
                                    }
                                }
                            }
                        },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color.White.copy(alpha = 0.10f),
                            contentColor = Color.White
                        ),
                        shape = RoundedCornerShape(10.dp),
                        contentPadding = PaddingValues(horizontal = 14.dp, vertical = 6.dp),
                        modifier = Modifier.padding(top = 6.dp)
                    ) {
                        Text("Attempt executable load (ELF/SELF)", fontSize = 11.sp)
                    }
                }
                loadResult?.let { res ->
                    Text(
                        text = res,
                        fontSize = 10.sp,
                        color = if (res.startsWith("LOADED")) Color(0xFF69F0AE) else Color(0xFFFF8A65),
                        fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                    )
                }
            }
        }

        // ---- overlay layer 5: pad edit toolbar -------------------------------
        if (padEditing && showPad) {
            Column(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .padding(bottom = 8.dp)
                    .clip(RoundedCornerShape(14.dp))
                    .background(Color.Black.copy(alpha = 0.78f))
                    .border(1.dp, Color(0xFFE2C74B).copy(alpha = 0.5f), RoundedCornerShape(14.dp))
                    .padding(horizontal = 14.dp, vertical = 8.dp)
            ) {
                Text(
                    "EDIT LAYOUT — drag any control; positions persist",
                    fontSize = 10.sp, color = Color(0xFFE2C74B),
                    fontFamily = TitilliumFontFamily
                )
                Row(verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    Text("Size ${padScale}%", fontSize = 10.sp,
                         color = Color.White, fontFamily = TitilliumFontFamily)
                    Slider(
                        value = padScale.toFloat(),
                        onValueChange = { Px5Settings.setPadScalePct(it.roundToInt()) },
                        valueRange = 60f..160f,
                        colors = SliderDefaults.colors(
                            thumbColor = Color(0xFF2E8CFF),
                            activeTrackColor = Color(0xFF2E8CFF)
                        ),
                        modifier = Modifier.width(140.dp)
                    )
                    Text("Opacity ${padOpacity}%", fontSize = 10.sp,
                         color = Color.White, fontFamily = TitilliumFontFamily)
                    Slider(
                        value = padOpacity.toFloat(),
                        onValueChange = { Px5Settings.setTouchOpacityPct(it.roundToInt()) },
                        valueRange = 40f..100f,
                        colors = SliderDefaults.colors(
                            thumbColor = Color(0xFF2E8CFF),
                            activeTrackColor = Color(0xFF2E8CFF)
                        ),
                        modifier = Modifier.width(140.dp)
                    )
                    Button(
                        onClick = {
                            padLayout = PadLayout.defaults()
                            Px5Settings.setPadLayoutJson("")
                        },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color.White.copy(alpha = 0.12f),
                            contentColor = Color.White
                        ),
                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp)
                    ) { Text("Reset", fontSize = 11.sp) }
                    Button(
                        onClick = { padEditing = false },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xFF2E8CFF),
                            contentColor = Color.White
                        ),
                        contentPadding = PaddingValues(horizontal = 14.dp, vertical = 4.dp)
                    ) { Text("Done", fontSize = 11.sp) }
                }
            }
        }
    }
}

/** Small rounded status chip for FPS / frametime. */
@Composable
private fun StatusChip(text: String, tint: Color) {
    Text(
        text = text,
        fontSize = 10.sp,
        color = tint,
        fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
        modifier = Modifier
            .clip(RoundedCornerShape(7.dp))
            .background(Color.Black.copy(alpha = 0.45f))
            .padding(horizontal = 7.dp, vertical = 3.dp)
    )
}

/** Process-wide marker: the boot diagnostics are one-shot per process. */
private object EmuScreenBoot {
    @Volatile var bootDiagnosticsDone: Boolean = false
}

// ---------------------------------------------------------------------------
// Honest eboot.bin probe for the boot-report line.
//
// Root cause (device log 2026-08-29): a plain java.io.File listing on API
// 30+ scoped storage returns null for SAF-imported game folders, and the
// old probe then printed "eboot=ABSENT" for a folder that CONTAINS
// eboot.bin. The probe is tiered:
//   1. Direct java.io listing (definitive when storage is readable).
//   2. Bounded recursion through the folder tree — v1.19: dump tools land
//      eboot.bin below the game root (decrypted/eboot.bin), and the
//      2026-08-30 v1.18 session showed an honest direct-children ABSENT
//      while a loadable dump sat one level down. ABSENT is claimed only
//      after the tree search misses.
//   3. SAF listing through persisted tree permissions.
//   4. "unknown(no-list-permission)" — we never fabricate ABSENT.
// ---------------------------------------------------------------------------
private fun probeEbootStatus(path: String, context: Context): String = try {
    val bootDir = java.io.File(path)
    if (bootDir.isFile) {
        "file(${bootDir.length()}B)"
    } else if (bootDir.isDirectory) {
        val listed = bootDir.listFiles()
        val direct = listed?.firstOrNull {
            it.isFile && it.name.equals("eboot.bin", true)
        }
        when {
            direct != null -> "present(${direct.length()}B)"
            listed != null -> {
                // Listing worked but no direct hit — search the bounded
                // tree before any ABSENT is claimed, and NAME where the
                // executable sits when it is found below the root.
                val nested = com.px5.emulator.EbootLocator.find(bootDir)
                if (nested != null) "present(${nested.file.length()}B,${nested.relPath})"
                else "ABSENT (folder tree searched, no eboot.bin)"
            }
            else -> probeEbootViaSaf(path, context)
                    ?: "unknown(no-list-permission)"
        }
    } else {
        probeEbootViaSaf(path, context) ?: "path-missing"
    }
} catch (_: Throwable) {
    "unknown(probe-error)"
}

/** SAF fallback: look for eboot.bin among the children of `path`'s
 *  document inside any persisted read-permission tree that contains it. */
private fun probeEbootViaSaf(path: String, context: Context): String? = runCatching {
    val cr = context.contentResolver
    for (perm in cr.persistedUriPermissions) {
        if (!perm.isReadPermission) continue
        val treeUri = perm.uri
        if (treeUri.scheme != "content") continue
        val treeDocId = DocumentsContract.getTreeDocumentId(treeUri)
        val root = treeDocId.substringBefore(':')
        if (root != "primary") continue
        val rootRest = treeDocId.substringAfter(':', "")
        val rootPath = if (rootRest.isBlank()) "/storage/emulated/0"
                       else "/storage/emulated/0/$rootRest"
        val normPath = path.trimEnd('/')
        val normRoot = rootPath.trimEnd('/')
        if (!(normPath == normRoot || normPath.startsWith("$normRoot/"))) continue

        val rel = if (normPath.length > normRoot.length)
            normPath.substring(normRoot.length + 1) else ""
        val targetDocId = if (rel.isBlank()) treeDocId
                          else "$treeDocId/$rel"
        val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                treeUri, targetDocId)
        cr.query(
            childrenUri,
            arrayOf(
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
                DocumentsContract.Document.COLUMN_SIZE),
            null, null, null)?.use { c ->
            while (c.moveToNext()) {
                val name = c.getString(0) ?: continue
                if (!name.equals("eboot.bin", true)) continue
                val mime = c.getString(1) ?: ""
                if (mime == DocumentsContract.Document.MIME_TYPE_DIR) continue
                val size = if (!c.isNull(2)) c.getLong(2) else -1L
                return@runCatching if (size >= 0) "present(${size}B,SAF)"
                                   else "present(SAF)"
            }
            return@runCatching "ABSENT(SAF)"
        }
    }
    null
}.getOrNull()
