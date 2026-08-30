package com.px5.emulator.ui

import android.view.SurfaceHolder
import android.view.SurfaceView
import android.content.Context
import android.provider.DocumentsContract
import com.px5.emulator.PhysicalControllerBridge
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.px5.emulator.core.FexCoreWrapper
import com.px5.emulator.core.Px5Settings
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.roundToInt
import kotlin.math.sqrt

// ---------------------------------------------------------------------------
// EmuScreen — REAL emulation-stage shell.
//
//  * hosts an actual SurfaceView whose Surface is handed to the Vulkan
//    swapchain renderer through JNI (attach -> start -> stop -> detach),
//  * shows a LIVE HUD polled from the C++ render loop (frames submitted,
//    present mode chosen from VSync setting, recreate counts),
//  * runs the honest offscreen GPU submission proof once when entering,
//  * provides an interactive touch DualSense-style pad whose presses land in
//    native InputManager atomics (round-trip proven via the summary line).
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

    var renderStats by remember { mutableStateOf("renderer idle") }
    var fpsText by remember { mutableStateOf("") }
    var frametimeText by remember { mutableStateOf("") }
    var gpuProof by remember { mutableStateOf<String?>(null) }
    var gnmSelfTest by remember { mutableStateOf<String?>(null) }
    var loaderSelfTest by remember { mutableStateOf<String?>(null) }
    var inputSummary by remember { mutableStateOf("input: -") }
    var loadResult by remember { mutableStateOf<String?>(null) }

    // Resolve the library entry behind this path (path is unique enough
    // for records the importer created) and credit REAL session time.
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
            // Session END must be visible: a pasted log that stops mid-play
            // previously gave no clue whether the user backed out or the
            // process died. game_exit with the exact session length closes
            // that ambiguity (a native crash additionally writes its FATAL
            // line via CrashHandler before the process goes away).
            com.px5.emulator.core.PX5EventLog.event(
                "gameBoot", "game_exit", "path=$path seconds=$seconds")
            try {
                fexCoreWrapper?.nativeLogEvent(
                    "gameBoot", "game_exit seconds=$seconds path=$path")
            } catch (_: Throwable) {
                // Engine may already be gone during teardown — the Kotlin
                // event above is the guaranteed record.
            }
        }
    }

    // Physical gamepad pass-through lives only while this screen exists.
    // Handheld consoles (built-in controls) and Bluetooth pads drive the
    // same native atomics as the on-screen overlay.
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

    // Virtual DualSense pad visibility (persisted; toggle in Input settings).
    val showPad by Px5Settings.showTouchPad.collectAsState()
    val padOpacity by Px5Settings.touchOpacityPct.collectAsState()
    val showFps by Px5Settings.showFps.collectAsState()
    val showFrametime by Px5Settings.showFrametime.collectAsState()

    // One-shot honest GPU proof (skipped on the UI-smoke x86_64 ABI).
    // Emits the boot story: on the 2026-08-29 paste the game screen produced
    // ZERO events between entry and process death — nothing to diagnose from.
    LaunchedEffect(Unit) {
        com.px5.emulator.core.PX5EventLog.event("gameBoot", "screen_entered",
                "path=$path stub=${isStubAbi}")
        // Same facts into the NATIVE log stream: a paste of the engine log
        // must show the game-boot path even if the process dies right after.
        try {
            val ebootStatus = probeEbootStatus(path, context)
            fexCoreWrapper?.nativeLogEvent("gameBoot",
                    "screen_entered game=${game?.name ?: "?"} titleId=${game?.titleId ?: "?"} " +
                    "format=${game?.format ?: "?"} size=${game?.sizeBytes ?: 0L} " +
                    "eboot=$ebootStatus")
        } catch (_: Throwable) {}
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
            // Real on BOTH ABIs: the GNM decoder is pure C++, no engine dep.
            gnmSelfTest = try {
                fexCoreWrapper?.nativeRunGnmSelfTest() ?: "wrapper missing"
            } catch (t: Throwable) { "FAIL | ${t.message}" }
            com.px5.emulator.core.PX5EventLog.event("gameBoot", "gnm_selftest",
                    "result=${gnmSelfTest?.lineSequence()?.firstOrNull() ?: "?"}")
        }
        launch(Dispatchers.Default) {
            // Real on BOTH ABIs: the SELF extractor is pure C++ too.
            loaderSelfTest = try {
                fexCoreWrapper?.nativeRunLoaderSelfTest() ?: "wrapper missing"
            } catch (t: Throwable) { "FAIL | ${t.message}" }
            com.px5.emulator.core.PX5EventLog.event("gameBoot", "loader_selftest",
                    "result=${loaderSelfTest?.lineSequence()?.firstOrNull() ?: "?"}")
        }
    }

    // Live HUD polling. FPS / frametime are computed from the engine's own
    // monotonic frame counter (frames= in the stats string) between polls —
    // no invented numbers: a stalled loop simply stops updating.
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

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .windowInsetsPadding(WindowInsets.statusBars)
                .padding(horizontal = 12.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(
                    onClick = {
                        onBackClick()
                    },
                    modifier = Modifier
                        .size(40.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.15f))
                ) {
                    Icon(
                        Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Exit",
                        tint = Color.White
                    )
                }
                Spacer(Modifier.width(10.dp))
                Column {
                    Text("PX5 Engine Stage", color = Color.White,
                         fontSize = 17.sp, fontWeight = FontWeight.Bold,
                         fontFamily = TitilliumFontFamily)
                    Text(text = path.substringAfterLast("/"),
                         color = Color(0xFF2E8CFF), fontSize = 12.sp,
                         fontFamily = TitilliumFontFamily, maxLines = 1)
                }
                Spacer(Modifier.weight(1f))
                IconButton(
                    onClick = { Px5Settings.setShowTouchPad(!showPad) },
                    modifier = Modifier
                        .size(40.dp)
                        .clip(CircleShape)
                        .background(
                            if (showPad) Color(0xFF0070D1).copy(alpha = 0.55f)
                            else Color.White.copy(alpha = 0.15f)
                        )
                ) {
                    Icon(
                        Icons.Default.SportsEsports,
                        contentDescription = "Toggle touch pad",
                        tint = Color.White
                    )
                }
            }

            // ---------------- live surface -----------------------------------
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(16f / 9f)
                    .clip(RoundedCornerShape(14.dp))
                    .border(2.dp, PS5AccentBlue, RoundedCornerShape(14.dp)),
                contentAlignment = Alignment.Center
            ) {
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
                    modifier = Modifier.fillMaxSize()
                )
            }

            Spacer(Modifier.height(8.dp))

            // ---------------- HUD chips --------------------------------------
            Text(
                text = renderStats,
                fontSize = 11.sp,
                color = Color(0xFF69F0AE),
                fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                textAlign = TextAlign.Start,
                modifier = Modifier.fillMaxWidth()
            )
            if (showFps && fpsText.isNotEmpty()) {
                Text(
                    text = fpsText,
                    fontSize = 11.sp,
                    color = Color(0xFF7DD3FC),
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
            }
            if (showFrametime && frametimeText.isNotEmpty()) {
                Text(
                    text = frametimeText,
                    fontSize = 11.sp,
                    color = Color(0xFF7DD3FC),
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
            }
            gpuProof?.let { p ->
                Text(
                    text = "GPU self-test: $p",
                    fontSize = 11.sp,
                    color = if (p.startsWith("PASS")) Color(0xFF7DD3FC)
                            else Color(0xFFFF8A65),
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
            }
            gnmSelfTest?.let { g ->
                Text(
                    text = "GNM PM4: ${g.lineSequence().firstOrNull() ?: g}",
                    fontSize = 11.sp,
                    color = if (g.startsWith("PASS")) Color(0xFF7DD3FC)
                            else Color(0xFFFF8A65),
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
            }
            loaderSelfTest?.let { s ->
                Text(
                    text = "SELF loader: ${s.lineSequence().firstOrNull() ?: s}",
                    fontSize = 11.sp,
                    color = if (s.startsWith("PASS")) Color(0xFF7DD3FC)
                            else Color(0xFFFF8A65),
                    fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                )
            }
            Text(
                text = inputSummary,
                fontSize = 11.sp,
                color = Color(0xFFE2C74B),
                fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
            )
            Text(
                text = "CPU bridge: $fexCoreStatus" +
                       (if (isStubAbi) "  •  UI-smoke ABI (engine=arm64-only)" else ""),
                fontSize = 11.sp,
                color = Color(0xFF9BA7BC)
            )

            // Honest boot attempt: hand the ELF to the real loader and
            // report the genuine result. Dumps use their eboot.bin.
            if (!isStubAbi && fexCoreWrapper != null) {
                Button(
                    onClick = {
                        scope.launch(Dispatchers.IO) {
                            val target = run {
                                val f = java.io.File(path)
                                if (f.isDirectory) {
                                    f.listFiles()?.firstOrNull {
                                        it.isFile && it.name.equals("eboot.bin", true)
                                    }?.absolutePath ?: ""
                                } else path
                            }
                            com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                    "exec_load_started", "target=$target")
                            fexCoreWrapper?.nativeLogEvent("gameBoot",
                                    "exec_load_started target=$target")
                            loadResult = if (target.isBlank()) {
                                com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                        "exec_load_failed", "reason=no eboot.bin in folder")
                                "LOAD FAILED: no eboot.bin in folder"
                            } else {
                                try {
                                    // v1.12 crash containment: the load
                                    // pipeline first runs inside a fork-
                                    // isolated probe child. A load-stage
                                    // native fault (the 2026-08-30 "game
                                    // kills the whole app" pattern) then
                                    // costs the probe child — the app
                                    // survives and the report names the
                                    // verified dump file. Only a probe that
                                    // actually succeeded promotes to the
                                    // real in-process mapping below.
                                    val probe = fexCoreWrapper.nativeLoadExecutableIsolated(target)
                                    com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                            "exec_probe",
                                            "target=${target.substringAfterLast('/')}",
                                            result = probe.lineSequence().firstOrNull()?.take(120) ?: "?")
                                    fexCoreWrapper.nativeLogEvent("gameBoot",
                                            "exec_probe ${probe.lineSequence().firstOrNull()?.take(160) ?: probe.take(160)} " +
                                            "target=${target.substringAfterLast('/')}")
                                    if (probe.startsWith("LOAD OK")) {
                                        // Magic-based dispatch on the native side:
                                        // SELF (eboot.bin dumps) -> extractor path,
                                        // ELF (homebrew payloads) -> direct loader.
                                        val ok = fexCoreWrapper.nativeLoadExecutable(target)
                                        com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                                "exec_load", "target=${target.substringAfterLast('/')}",
                                                result = ok.toString())
                                        fexCoreWrapper.nativeLogEvent("gameBoot",
                                                "exec_load result=$ok target=${target.substringAfterLast('/')}")
                                        if (ok) {
                                            // v1.13 execution containment: the real
                                            // guest execution runs in a fork-isolated
                                            // probe child (headless, hard 8s timeout).
                                            // A fault costs the child + a verified
                                            // dump — the app survives and shows it.
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
                                        // Probe crashed or rejected the file:
                                        // surface the FULL report (it names the
                                        // verified dump path on a crash) and do
                                        // NOT attempt the in-process load.
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
                    shape = RoundedCornerShape(10.dp)
                ) {
                    Text("Attempt executable load (ELF/SELF, experimental)", fontSize = 12.sp)
                }
                loadResult?.let { res ->
                    Text(
                        text = res,
                        fontSize = 11.sp,
                        color = if (res.startsWith("LOADED")) Color(0xFF69F0AE) else Color(0xFFFF8A65),
                        fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                    )
                }
            }

            Spacer(Modifier.weight(1f))

            // ------------- virtual DualSense pad ---------------------------
            // Every control below lands in PX5::InputManager atomics over
            // JNI: buttons via nativeSetButtonState, sticks via
            // nativeSetLeftStick/nativeSetRightStick, triggers via
            // nativeSetTriggers, touchpad via nativeSetTouchpad. The
            // "input:" HUD line above proves events reach C++.
            if (showPad) {
                Box(modifier = Modifier.alpha(padOpacity / 100f)) {
                    DualSenseOverlay(
                        enabled = !isStubAbi,
                        onButton = { bit, down ->
                            fexCoreWrapper?.nativeSetButtonState(bit, down)
                        },
                        onLeftStick = { x, y -> fexCoreWrapper?.nativeSetLeftStick(x, y) },
                        onRightStick = { x, y -> fexCoreWrapper?.nativeSetRightStick(x, y) },
                        onTriggers = { l2, r2 -> fexCoreWrapper?.nativeSetTriggers(l2, r2) },
                        onTouchpad = { down -> fexCoreWrapper?.nativeSetTouchpad(down) }
                    )
                }
            }
            Spacer(Modifier.height(10.dp))
        }
    }
}

// ---------------------------------------------------------------------------
// DualSenseOverlay — the full on-screen controller.
//
// Layout mirrors the real pad: L2/L1 shoulders left, R1/R2 right,
// D-pad cross left cluster, face-button diamond right cluster, analog
// sticks inboard, Options/Share/PS + clickable touchpad in the middle.
// ---------------------------------------------------------------------------
@Composable
private fun DualSenseOverlay(
    enabled: Boolean,
    onButton: (Int, Boolean) -> Unit,
    onLeftStick: (Float, Float) -> Unit,
    onRightStick: (Float, Float) -> Unit,
    onTriggers: (Float, Float) -> Unit,
    onTouchpad: (Boolean) -> Unit
) {
    val l2 = remember { mutableStateOf(0f) }
    val r2 = remember { mutableStateOf(0f) }

    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        // ---- shoulder row -------------------------------------------------
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth()
        ) {
            ShoulderKey("L2", enabled, onPress = {
                l2.value = if (it) 1f else 0f
                onTriggers(l2.value, r2.value)
            })
            Spacer(Modifier.width(6.dp))
            ShoulderKey("L1", enabled, onPress = {
                onButton(FexCoreWrapper.PAD_L1, it)
            })
            Spacer(Modifier.weight(1f))
            // Clickable touchpad (center) — real native state.
            PadKey("TP", Color(0xFF9BA7BC), enabled, 40.dp, onPress = onTouchpad)
            Spacer(Modifier.weight(1f))
            ShoulderKey("R1", enabled, onPress = {
                onButton(FexCoreWrapper.PAD_R1, it)
            })
            Spacer(Modifier.width(6.dp))
            ShoulderKey("R2", enabled, onPress = {
                r2.value = if (it) 1f else 0f
                onTriggers(l2.value, r2.value)
            })
        }

        // ---- main row ------------------------------------------------------
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth()
        ) {
            DPadCross(enabled, onButton)

            Spacer(Modifier.width(14.dp))
            AnalogStick("L", enabled, onLeftStick)

            Spacer(Modifier.weight(1f))

            Column(horizontalAlignment = Alignment.CenterHorizontally,
                   verticalArrangement = Arrangement.spacedBy(6.dp)) {
                PadKey("\u22EE", Color(0xFF9BA7BC), enabled, 38.dp, onPress = {
                    onButton(FexCoreWrapper.PAD_OPTIONS, it)
                })
                PadKey("PS", Color(0xFF9BA7BC), enabled, 38.dp, onPress = {
                    onButton(FexCoreWrapper.PAD_PS_HOME, it)
                })
                PadKey("\u2913", Color(0xFF9BA7BC), enabled, 38.dp, onPress = {
                    onButton(FexCoreWrapper.PAD_SHARE, it)
                })
            }

            Spacer(Modifier.weight(1f))

            AnalogStick("R", enabled, onRightStick)
            Spacer(Modifier.width(14.dp))
            FaceDiamond(enabled, onButton)
        }
    }
}

/** Slim shoulder/trigger bar. Triggers report analog 1.0 while held. */
@Composable
private fun ShoulderKey(
    label: String,
    enabled: Boolean,
    onPress: (Boolean) -> Unit
) {
    var pressed by remember { mutableStateOf(false) }
    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .size(width = 62.dp, height = 28.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(
                if (pressed) Color(0xFF2E8CFF).copy(alpha = 0.45f)
                else Color.White.copy(alpha = 0.06f)
            )
            .border(
                1.5.dp,
                if (pressed) Color(0xFF2E8CFF) else Color.White.copy(alpha = 0.3f),
                RoundedCornerShape(8.dp)
            )
            .then(
                if (enabled) {
                    Modifier.pointerInput(label) {
                        detectTapGestures(onPress = {
                            pressed = true
                            onPress(true)
                            try { awaitRelease() } catch (_: Throwable) {}
                            pressed = false
                            onPress(false)
                        })
                    }
                } else Modifier
            )
    ) {
        Text(label, color = Color.White, fontSize = 11.sp, fontWeight = FontWeight.Bold)
    }
}

/** Draggable analog stick; reports normalized [-1..1], up = -1 (standard). */
@Composable
private fun AnalogStick(
    label: String,
    enabled: Boolean,
    onMove: (Float, Float) -> Unit,
    size: Dp = 96.dp
) {
    var thumb by remember { mutableStateOf(Offset.Zero) }
    val density = LocalDensity.current
    val thumbSize = 36.dp
    val maxPx = with(density) { (size - thumbSize).toPx() / 2f }

    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .size(size)
            .clip(CircleShape)
            .background(Color.White.copy(alpha = 0.05f))
            .border(1.5.dp, Color.White.copy(alpha = 0.25f), CircleShape)
            .then(
                if (enabled) {
                    Modifier.pointerInput(Unit) {
                        detectDragGestures(
                            onDrag = { change, dragAmount ->
                                change.consume()
                                val v = thumb + dragAmount
                                val d = sqrt(v.x * v.x + v.y * v.y)
                                thumb = if (d > maxPx && d > 0f) v * (maxPx / d) else v
                                onMove(thumb.x / maxPx, thumb.y / maxPx)
                            },
                            onDragEnd = {
                                thumb = Offset.Zero
                                onMove(0f, 0f)
                            },
                            onDragCancel = {
                                thumb = Offset.Zero
                                onMove(0f, 0f)
                            }
                        )
                    }
                } else Modifier
            )
    ) {
        Text(
            label,
            color = Color.White.copy(alpha = 0.3f),
            fontSize = 11.sp,
            fontWeight = FontWeight.Bold
        )
        Box(
            modifier = Modifier
                .offset { IntOffset(thumb.x.roundToInt(), thumb.y.roundToInt()) }
                .size(thumbSize)
                .clip(CircleShape)
                .background(
                    if (enabled) Color(0xFF2E8CFF).copy(alpha = 0.85f)
                    else Color.White.copy(alpha = 0.15f)
                )
                .border(1.dp, Color.White.copy(alpha = 0.5f), CircleShape)
        )
    }
}

/** D-pad cross (up/left/right/down). */
@Composable
private fun DPadCross(
    enabled: Boolean,
    onButton: (Int, Boolean) -> Unit
) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        PadKey("\u25B2", Color.White, enabled, 44.dp, onPress = {
            onButton(FexCoreWrapper.PAD_DPAD_UP, it)
        })
        Row(verticalAlignment = Alignment.CenterVertically) {
            PadKey("\u25C0", Color.White, enabled, 44.dp, onPress = {
                onButton(FexCoreWrapper.PAD_DPAD_LEFT, it)
            })
            Spacer(Modifier.width(4.dp))
            PadKey("\u25B6", Color.White, enabled, 44.dp, onPress = {
                onButton(FexCoreWrapper.PAD_DPAD_RIGHT, it)
            })
        }
        PadKey("\u25BC", Color.White, enabled, 44.dp, onPress = {
            onButton(FexCoreWrapper.PAD_DPAD_DOWN, it)
        })
    }
}

/** Face-button diamond: △ top, ○ right, ✕ bottom, □ left. */
@Composable
private fun FaceDiamond(
    enabled: Boolean,
    onButton: (Int, Boolean) -> Unit
) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        PadKey("\u25B3", Color(0xFF4CD9A6), enabled, 46.dp, onPress = {
            onButton(FexCoreWrapper.PAD_TRIANGLE, it)
        })
        Row(verticalAlignment = Alignment.CenterVertically) {
            PadKey("\u25A1", Color(0xFFEF7DBD), enabled, 46.dp, onPress = {
                onButton(FexCoreWrapper.PAD_SQUARE, it)
            })
            Spacer(Modifier.width(6.dp))
            PadKey("\u25CB", Color(0xFFFF6B6B), enabled, 46.dp, onPress = {
                onButton(FexCoreWrapper.PAD_CIRCLE, it)
            })
        }
        PadKey("\u00D7", Color(0xFF7FB8FF), enabled, 46.dp, onPress = {
            onButton(FexCoreWrapper.PAD_CROSS, it)
        })
    }
}

/**
 * Round pad key. Every press/release goes through the caller into
 * FexCoreWrapper JNI; per-key pointerInput keeps multi-touch working
 * (hold D-pad while dragging a stick, etc.).
 */
@Composable
private fun PadKey(
    label: String,
    tint: Color,
    enabled: Boolean,
    size: Dp,
    onPress: (Boolean) -> Unit
) {
    var pressed by remember { mutableStateOf(false) }
    val shape = CircleShape

    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .size(size)
            .clip(shape)
            .background(if (pressed) tint.copy(alpha = 0.35f)
                        else Color.White.copy(alpha = 0.06f))
            .border(1.5.dp, if (pressed) tint else tint.copy(alpha = 0.5f), shape)
            .then(if (enabled) {
                Modifier.pointerInput(label) {
                    detectTapGestures(onPress = {
                        pressed = true
                        onPress(true)
                        try { awaitRelease() } catch (_: Throwable) {}
                        pressed = false
                        onPress(false)
                    })
                }
            } else Modifier)
    ) {
        Text(label, color = tint, fontSize = 12.sp, fontWeight = FontWeight.Bold)
    }
}

// ---------------------------------------------------------------------------
// Honest eboot.bin probe for the boot-report line.
//
// Root cause (device log 2026-08-29): a plain java.io.File listing on API
// 30+ scoped storage returns null for SAF-imported game folders, and the
// old probe then printed "eboot=ABSENT" for a folder that CONTAINS
// eboot.bin (the competitor log ran that exact file from that exact path).
// A report line that lies is worse than no line — the probe is now tiered:
//   1. Direct java.io listing (definitive when storage is readable).
//   2. SAF listing through persisted tree permissions (covers SAF-imported
//      games: docId "primary:<rest>" maps to /storage/emulated/0/<rest>).
//   3. "unknown(no-list-permission)" — we never fabricate ABSENT.
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
            listed != null -> "ABSENT"   // listing worked; genuinely absent
            else -> probeEbootViaSaf(path, context)
                    ?: "unknown(no-list-permission)"
        }
    } else {
        // File API cannot even see the node — SAF is the only witness left.
        probeEbootViaSaf(path, context) ?: "path-missing"
    }
} catch (_: Throwable) {
    "unknown(probe-error)"
}

/** SAF fallback: look for eboot.bin among the children of `path`'s
 *  document inside any persisted read-permission tree that contains it.
 *  Returns null when no persisted tree covers the path. */
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
        // Children of the game-folder document: (treeUri, parentDocId).
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
