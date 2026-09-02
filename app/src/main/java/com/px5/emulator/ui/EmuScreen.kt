package com.px5.emulator.ui

import android.view.SurfaceHolder
import android.view.SurfaceView
import android.content.Context
import android.content.pm.ActivityInfo
import android.provider.DocumentsContract
import com.px5.emulator.PhysicalControllerBridge
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.activity.compose.BackHandler
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Description
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.ExitToApp
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Refresh
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
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.px5.emulator.core.FexCoreWrapper
import com.px5.emulator.core.Px5Settings
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
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
    onBackClick: () -> Unit,
    onOpenLogs: () -> Unit = {}
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
    // v1.32 — the boot is a PIPELINE with one honest progress bar (0..100),
    // not a diagnostics wall over the game surface. Stages: engine prep →
    // once-per-version CPU experiments → driver/loader self-tests → eboot
    // locate → isolated image verification → in-process map → execution
    // probe → RUNNING. All engine evidence still lands in the unified log
    // and is read from the dedicated Logs screen (Dolphin/sharpdroid way).
    var bootStage by remember { mutableStateOf("Preparing engine") }
    var bootProgress by remember { mutableFloatStateOf(0.02f) }
    var bootDone by remember { mutableStateOf(false) }
    var bootError by remember { mutableStateOf<String?>(null) }
    var bootRetryToken by remember { mutableIntStateOf(0) }
    var sessionReport by remember { mutableStateOf<String?>(null) }
    // v1.26: set once by the screen-entry eboot probe. A folder without
    // eboot.bin fails the pipeline with ONE clear message instead of N
    // repeated empty-target exec_load_failed rows.
    var ebootMissing by remember { mutableStateOf(false) }
    // v1.32 — Eden-style in-game menu: back / PS button open it, never a
    // raw text panel over the game.
    var menuOpen by remember { mutableStateOf(false) }
    var padEditing by remember { mutableStateOf(false) }

    // v1.36 — Eden semantics for the system back gesture: it IS the console
    // button. In-game it opens the pause menu (or resumes from it); during
    // boot/failure it leaves the stage; in pad-edit mode it leaves editing.
    // The old floating top-bar buttons are gone — this gesture plus the pad's
    // PS button are the only stage-level controls, matching Eden/Vita3K.
    BackHandler {
        when {
            padEditing -> padEditing = false
            menuOpen -> menuOpen = false          // back in menu == resume
            bootDone && bootError == null -> menuOpen = true
            else -> onBackClick()
        }
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

    // ---- v1.32 BOOT PIPELINE (sequential, one honest progress bar) --------
    // Replaces the v1.16..v1.31 parallel probe fan-out + manual load
    // button. Everything runs in ONE coroutine so progress stages are
    // truthful (no stage jumps), and the once-per-version CPU experiments
    // complete BEFORE the game image is mapped into the shared guest
    // window (the DYN-style game base and the foundation fixtures share
    // the window anchor — concurrent mapping would corrupt the game text
    // page; the vc32 session never hit this only because the suite died
    // before any game load existed).
    LaunchedEffect(Unit) {
        com.px5.emulator.core.PX5EventLog.event("gameBoot", "screen_entered",
                "path=$path stub=${isStubAbi} diagRan=${EmuScreenBoot.bootDiagnosticsDone}")
        try {
            val ebootStatus = probeEbootStatus(path, context)
            ebootMissing = ebootStatus.startsWith("ABSENT")
            fexCoreWrapper?.nativeLogEvent("gameBoot",
                    "screen_entered game=${game?.name ?: "?"} titleId=${game?.titleId ?: "?"} " +
                    "format=${game?.format ?: "?"} size=${game?.sizeBytes ?: 0L} " +
                    "eboot=$ebootStatus")
        } catch (_: Throwable) {}
    }

    LaunchedEffect(bootRetryToken) {
        fun setStage(p: Float, label: String) {
            bootProgress = p
            bootStage = label
        }
        bootDone = false
        bootError = null
        sessionReport = null
        bootProgress = 0.02f
        bootStage = "Preparing engine"
        if (isStubAbi || fexCoreWrapper == null) {
            // UI-smoke ABI: no engine — surface the stage honestly and stop.
            bootProgress = 1f
            bootDone = true
            return@LaunchedEffect
        }
        val wrapper = fexCoreWrapper
        val currentVc = try {
            context.packageManager.getPackageInfo(context.packageName, 0)
                .longVersionCode.toInt()
        } catch (_: Throwable) { 0 }

        // -- stage 1: once-per-version CPU experiments (in-process, safe
        //    since v1.24; flags written BEFORE the run — no crash loops) --
        val prefs = context.getSharedPreferences(
                "px5_engine_settings", android.content.Context.MODE_PRIVATE)
        if (EmuScreenBoot.conformanceDecidedVc != currentVc) {
            EmuScreenBoot.conformanceDecidedVc = currentVc
            if (prefs.getInt("cpuGateInprocVc", -1) == currentVc) {
                com.px5.emulator.core.PX5EventLog.event("conformance", "auto_run",
                        "skipped — already ran for vc=$currentVc " +
                        "(verdict: see the log / crash report of the session " +
                        "that first ran this build)")
            } else {
                setStage(0.10f, "Checking CPU bridge")
                prefs.edit().putInt("cpuGateInprocVc", currentVc).apply()
                com.px5.emulator.core.PX5EventLog.event("conformance", "auto_run",
                        "STARTING once for vc=$currentVc (in-process)")
                val rep = withContext(Dispatchers.Default) {
                    try {
                        wrapper.nativeRunCpuConformanceInProcess()
                    } catch (t: Throwable) { "FAILED — ${t.message}" }
                }
                com.px5.emulator.core.PX5EventLog.event("conformance",
                        "inprocess_result",
                        "result=${rep.lineSequence().firstOrNull() ?: "?"}")
                try {
                    wrapper.nativeLogEvent("conformance",
                            "in-process conformance: ${rep.take(500)}")
                } catch (_: Throwable) {}

                if (rep.startsWith("PASSED") &&
                    prefs.getInt("foundationTestVc", -1) != currentVc) {
                    prefs.edit().putInt("foundationTestVc", currentVc).apply()
                    setStage(0.16f, "Running foundation suite")
                    com.px5.emulator.core.PX5EventLog.event("foundation",
                            "auto_run", "STARTING once for vc=$currentVc — " +
                            "in-process, no fork: 10-step foundation suite.")
                    val frep = withContext(Dispatchers.Default) {
                        try {
                            wrapper.nativeRunFoundationSelfTestInProcess()
                        } catch (t: Throwable) { "VERDICT: FAILED (${t.message})" }
                    }
                    val verdictLine = frep.lineSequence().firstOrNull {
                        it.startsWith("VERDICT:")
                    } ?: "VERDICT: ABSENT (report truncated)"
                    com.px5.emulator.core.PX5EventLog.event("foundation",
                            "result", verdictLine)
                    try {
                        wrapper.nativeLogEvent("foundation",
                                "foundation self-test (in-process): " +
                                frep.take(1500))
                    } catch (_: Throwable) {}
                }
            }
        }

        // -- stage 2: driver / loader self-tests (once per process) -------
        if (EmuScreenBoot.bootDiagnosticsDone) {
            gpuProof = "SKIP | already ran this process (see Logs)"
        } else {
            EmuScreenBoot.bootDiagnosticsDone = true
            setStage(0.24f, "Testing GPU driver")
            gpuProof = withContext(Dispatchers.Default) {
                try { wrapper.nativeRunGpuProof() } catch (t: Throwable) { "FAIL | ${t.message}" }
            }
            com.px5.emulator.core.PX5EventLog.event("gameBoot", "gpu_proof",
                    "result=${gpuProof?.take(120)}")
            setStage(0.34f, "Testing GNM decoder")
            gnmSelfTest = withContext(Dispatchers.Default) {
                try { wrapper.nativeRunGnmSelfTest() } catch (t: Throwable) { "FAIL | ${t.message}" }
            }
            com.px5.emulator.core.PX5EventLog.event("gameBoot", "gnm_selftest",
                    "result=${gnmSelfTest?.lineSequence()?.firstOrNull() ?: "?"}")
            setStage(0.42f, "Testing SELF loader")
            loaderSelfTest = withContext(Dispatchers.Default) {
                try { wrapper.nativeRunLoaderSelfTest() } catch (t: Throwable) { "FAIL | ${t.message}" }
            }
            com.px5.emulator.core.PX5EventLog.event("gameBoot", "loader_selftest",
                    "result=${loaderSelfTest?.lineSequence()?.firstOrNull() ?: "?"}")
        }

        // -- stage 3: locate eboot.bin (java.io + SAF) --------------------
        setStage(0.50f, "Locating eboot.bin")
        if (ebootMissing) {
            bootError = "This folder has no eboot.bin. A valid PS5 game " +
                    "dump (with eboot.bin) is required to start."
            return@LaunchedEffect
        }
        val target: String = withContext(Dispatchers.IO) {
            val f = java.io.File(path)
            val io = when {
                f.isDirectory ->
                    com.px5.emulator.EbootLocator.find(f)?.also {
                        com.px5.emulator.core.PX5EventLog.event("gameBoot",
                                "exec_target",
                                "rel=${it.relPath} depth=${it.depth} dirs=${it.dirsVisited}")
                    }?.file?.absolutePath
                f.isFile -> path
                else -> null
            }
            io ?: safCopyTarget(path, context)?.let { ct ->
                com.px5.emulator.core.PX5EventLog.event("gameBoot",
                        "exec_target", ct.second)
                ct.first.absolutePath
            } ?: ""
        }
        com.px5.emulator.core.PX5EventLog.event("gameBoot",
                "exec_load_started", "target=$target")
        try {
            wrapper.nativeLogEvent("gameBoot", "exec_load_started target=$target")
        } catch (_: Throwable) {}
        if (target.isBlank()) {
            bootError = "Could not resolve an eboot.bin target after the " +
                    "java.io and SAF searches (see Logs)."
            return@LaunchedEffect
        }

        // -- stage 4: isolated image verification -------------------------
        setStage(0.60f, "Verifying game image")
        val probe = withContext(Dispatchers.Default) {
            try {
                wrapper.nativeLoadExecutableIsolated(target)
            } catch (t: Throwable) {
                com.px5.emulator.core.PX5EventLog.exception("gameBoot.execLoad", t)
                "LOAD FAILED: ${t.message}"
            }
        }
        com.px5.emulator.core.PX5EventLog.event("gameBoot",
                "exec_probe",
                "target=${target.substringAfterLast('/')}",
                result = probe.lineSequence().firstOrNull()?.take(120) ?: "?")
        try {
            wrapper.nativeLogEvent("gameBoot",
                    "exec_probe ${probe.lineSequence().firstOrNull()?.take(160) ?: probe.take(160)} " +
                    "target=${target.substringAfterLast('/')}")
        } catch (_: Throwable) {}
        if (!probe.startsWith("LOAD OK")) {
            bootError = "The image was refused by the loader: " +
                    (probe.lineSequence().firstOrNull() ?: "unknown reason") +
                    " — full evidence in Logs."
            return@LaunchedEffect
        }

        // -- stage 5: map for real (this process executes it) -------------
        setStage(0.75f, "Mapping game image")
        val ok = withContext(Dispatchers.Default) {
            try {
                wrapper.nativeLoadExecutable(target)
            } catch (t: Throwable) { false }
        }
        com.px5.emulator.core.PX5EventLog.event("gameBoot",
                "exec_load", "target=${target.substringAfterLast('/')}",
                result = ok.toString())
        try {
            wrapper.nativeLogEvent("gameBoot",
                    "exec_load result=$ok target=${target.substringAfterLast('/')}")
        } catch (_: Throwable) {}
        if (!ok) {
            bootError = "The loader rejected $target during the real map " +
                    "(see Logs)."
            return@LaunchedEffect
        }

        // -- stage 6: execution probe (contained, once per process/path) --
        if (EmuScreenBoot.execProbedPath != target) {
            EmuScreenBoot.execProbedPath = target
            setStage(0.85f, "Starting game")
            val exec = withContext(Dispatchers.Default) {
                try {
                    wrapper.nativeRunExecutionProbe(target, 8000)
                } catch (t: Throwable) { "EXEC FAILED: ${t.message}" }
            }
            com.px5.emulator.core.PX5EventLog.event("gameBoot",
                    "exec_probe_run",
                    "target=${target.substringAfterLast('/')}",
                    result = exec.lineSequence().firstOrNull()?.take(120) ?: "?")
            try {
                wrapper.nativeLogEvent("gameBoot",
                        "exec_probe_run ${exec.lineSequence().firstOrNull()?.take(160) ?: exec.take(160)}")
            } catch (_: Throwable) {}
            if (exec.contains("CRASHED") || exec.startsWith("EXEC FAILED")) {
                // Contained by design: the app survives, evidence is on
                // disk. Surface a chip, not a wall of red text.
                sessionReport = (exec.lineSequence().firstOrNull() ?: exec).take(120)
            }
        }

        setStage(1f, "Running")
        delay(350)
        bootDone = true
    }

    // Gentle trickle so long native stages never look frozen.
    LaunchedEffect(bootDone, bootError) {
        while (!bootDone && bootError == null && bootProgress < 0.84f) {
            delay(500)
            bootProgress = (bootProgress + 0.004f).coerceAtMost(0.84f)
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

    // ---- v1.32 console gestures --------------------------------------------
    // System back NEVER kills the session directly: it opens/toggles the
    // in-game menu (Eden behavior). Exiting lives in the menu.
    androidx.activity.compose.BackHandler(enabled = true) {
        when {
            padEditing -> padEditing = false
            bootError != null -> onBackClick()
            else -> menuOpen = !menuOpen
        }
    }

    val animatedBootProgress by animateFloatAsState(
        targetValue = bootProgress.coerceIn(0f, 1f),
        animationSpec = tween(durationMillis = 300),
        label = "bootProgress"
    )

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
                    // v1.32: the PS home button is the console gesture —
                    // it opens the in-game menu (Eden behavior) instead of
                    // reaching the guest.
                    if (bit == FexCoreWrapper.PAD_PS_HOME && down) {
                        menuOpen = !menuOpen
                    } else {
                        fexCoreWrapper?.nativeSetButtonState(bit, down)
                    }
                },
                onLeftStick = { x, y -> fexCoreWrapper?.nativeSetLeftStick(x, y) },
                onRightStick = { x, y -> fexCoreWrapper?.nativeSetRightStick(x, y) },
                onTriggers = { l2, r2 -> fexCoreWrapper?.nativeSetTriggers(l2, r2) },
                onTouchpad = { down -> fexCoreWrapper?.nativeSetTouchpad(down) }
            )
        }

        // v1.36 — overlay layer 2 is GONE. The floating top bar (back arrow,
        // game title, pad toggle, pad-edit pencil) lived in places that
        // covered play surface and felt like launcher chrome over a console
        // game. Everything it did lives in the pause menu (PS button or the
        // back gesture), which already carries Resume / Logs / pad controls /
        // Exit — Eden semantics. The FPS/frametime counters moved into that
        // menu too (see EmuInGameMenu).

        // ---- overlay layer 3: boot loading overlay (0 -> 100, branded) -----
        androidx.compose.animation.AnimatedVisibility(
            visible = !bootDone || bootError != null,
            enter = fadeIn(tween(250)),
            exit = fadeOut(tween(450)),
            modifier = Modifier.matchParentSize()
        ) {
            EmuBootLoading(
                gameName = game?.name ?: path.substringAfterLast('/'),
                titleId = game?.titleId ?: "",
                coverPath = game?.coverPath ?: "",
                stage = bootStage,
                progress01 = animatedBootProgress,
                error = bootError,
                onRetry = { bootRetryToken++ },
                onOpenLogs = onOpenLogs,
                onExit = onBackClick
            )
        }

        // ---- overlay layer 4: in-game menu (Eden-style pause panel) --------
        androidx.compose.animation.AnimatedVisibility(
            visible = menuOpen,
            enter = fadeIn(tween(150)),
            exit = fadeOut(tween(150)),
            modifier = Modifier.matchParentSize()
        ) {
            EmuInGameMenu(
                gameName = game?.name ?: path.substringAfterLast('/'),
                padShown = showPad,
                sessionReport = sessionReport,
                fpsText = if (showFps) fpsText else "",
                frametimeText = if (showFrametime) frametimeText else "",
                onResume = { menuOpen = false },
                onTogglePad = {
                    Px5Settings.setShowTouchPad(!showPad)
                    menuOpen = false
                },
                onEditPad = {
                    if (showPad) padEditing = true
                    menuOpen = false
                },
                onOpenLogs = {
                    menuOpen = false
                    onOpenLogs()
                },
                onExit = onBackClick
            )
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

// ---------------------------------------------------------------------------
// v1.35 — the boot experience. The game's OWN cover card leads the screen
// (library cover at the home-card size, game name, title id, honest stage
// label, smooth 0..100 bar), modeled on what Eden/Vita3K show between
// "Play" and the first frame. Failure states show only the symbolic title
// id plus one clean centered card ("Couldn't start the game") with named
// actions — the detailed reason lives in the Logs screen, never here.
// ---------------------------------------------------------------------------
@Composable
private fun EmuBootLoading(
    gameName: String,
    titleId: String,
    coverPath: String,
    stage: String,
    progress01: Float,
    error: String?,
    onRetry: () -> Unit,
    onOpenLogs: () -> Unit,
    onExit: () -> Unit
) {
    val c = px5Colors()
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    listOf(Color(0xFF05070B), Color(0xFF0E1218))
                )
            ),
        contentAlignment = Alignment.Center
    ) {
        if (error == null) {
            // ---- booting: the game's own card + the one honest bar --------
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier.padding(horizontal = 40.dp)
            ) {
                BootGameCover(gameName = gameName, coverPath = coverPath)
                Spacer(Modifier.height(14.dp))
                Text(
                    text = gameName,
                    color = c.text,
                    fontSize = 16.sp,
                    fontWeight = FontWeight.SemiBold,
                    fontFamily = TitilliumFontFamily,
                    textAlign = TextAlign.Center,
                    maxLines = 2
                )
                if (titleId.isNotEmpty()) {
                    Spacer(Modifier.height(3.dp))
                    Text(
                        text = titleId,
                        color = c.textSecondary,
                        fontSize = 11.sp,
                        fontFamily = TitilliumFontFamily
                    )
                }
                Spacer(Modifier.height(26.dp))
                // ---- the bar ------------------------------------------------
                Box(
                    modifier = Modifier
                        .width(264.dp)
                        .height(5.dp)
                        .clip(RoundedCornerShape(3.dp))
                        .background(Color.White.copy(alpha = 0.10f))
                ) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth(progress01.coerceIn(0f, 1f))
                            .height(5.dp)
                            .clip(RoundedCornerShape(3.dp))
                            .background(
                                Brush.horizontalGradient(
                                    listOf(PS5AccentBlue, Color(0xFF2E8CFF))
                                )
                            )
                    )
                }
                Spacer(Modifier.height(10.dp))
                Row(
                    modifier = Modifier.width(264.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = stage,
                        color = c.textSecondary,
                        fontSize = 12.sp,
                        fontFamily = TitilliumFontFamily,
                        modifier = Modifier.weight(1f)
                    )
                    Text(
                        text = "${(progress01.coerceIn(0f, 1f) * 100).roundToInt()}%",
                        color = Color.White.copy(alpha = 0.85f),
                        fontSize = 12.sp,
                        fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
                    )
                }
            }
        } else {
            // ---- failure: only the symbolic title id + one clean card -----
            // (the detailed reason is in the Logs screen — never here)
            Column(
                horizontalAlignment = Alignment.CenterHorizontally,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 24.dp)
            ) {
                if (titleId.isNotEmpty()) {
                    Text(
                        text = titleId,
                        color = c.textSecondary,
                        fontSize = 13.sp,
                        fontFamily = TitilliumFontFamily
                    )
                    Spacer(Modifier.height(22.dp))
                }
                Column(
                    modifier = Modifier
                        .fillMaxWidth(0.5f)
                        .widthIn(min = 320.dp)
                        .clip(RoundedCornerShape(16.dp))
                        .background(c.sheet)
                        .border(1.dp, c.danger.copy(alpha = 0.45f), RoundedCornerShape(16.dp))
                        .padding(horizontal = 18.dp, vertical = 22.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text(
                        "Couldn't start the game",
                        color = c.text, fontSize = 15.sp,
                        fontWeight = FontWeight.SemiBold,
                        fontFamily = TitilliumFontFamily,
                        textAlign = TextAlign.Center
                    )
                    // v1.35: the loader's detailed reason is NOT printed here —
                    // the Logs screen carries the full evidence.
                    Spacer(Modifier.height(16.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        Button(
                            onClick = onRetry,
                            colors = ButtonDefaults.buttonColors(
                                containerColor = PS5AccentBlue,
                                contentColor = Color.White
                            ),
                            shape = RoundedCornerShape(10.dp),
                            contentPadding = PaddingValues(horizontal = 14.dp, vertical = 6.dp)
                        ) {
                            Icon(Icons.Default.Refresh, contentDescription = null,
                                 modifier = Modifier.size(14.dp))
                            Spacer(Modifier.width(5.dp))
                            Text("Retry", fontSize = 12.sp)
                        }
                        Button(
                            onClick = onOpenLogs,
                            colors = ButtonDefaults.buttonColors(
                                containerColor = c.control,
                                contentColor = c.text
                            ),
                            shape = RoundedCornerShape(10.dp),
                            contentPadding = PaddingValues(horizontal = 14.dp, vertical = 6.dp)
                        ) {
                            Icon(Icons.Default.Description, contentDescription = null,
                                 modifier = Modifier.size(14.dp))
                            Spacer(Modifier.width(5.dp))
                            Text("Logs", fontSize = 12.sp)
                        }
                        Button(
                            onClick = onExit,
                            colors = ButtonDefaults.buttonColors(
                                containerColor = c.control,
                                contentColor = c.danger
                            ),
                            shape = RoundedCornerShape(10.dp),
                            contentPadding = PaddingValues(horizontal = 14.dp, vertical = 6.dp)
                        ) {
                            Text("Back", fontSize = 12.sp)
                        }
                    }
                }
            }
        }
    }
}

/** v1.35 — the boot screen's game identity: the library cover at the
 *  home-card size; an initials tile when no cover was imported. */
@Composable
private fun BootGameCover(gameName: String, coverPath: String) {
    val c = px5Colors()
    Box(
        modifier = Modifier
            .size(120.dp, 170.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(Color(0xFF121822))
            .border(1.dp, c.hairline, RoundedCornerShape(12.dp)),
        contentAlignment = Alignment.Center
    ) {
        val cover = rememberGameCover(coverPath)
        if (cover != null) {
            Image(
                bitmap = cover,
                contentDescription = gameName,
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize()
            )
        } else {
            Text(
                text = gameName.take(2).uppercase(),
                color = c.textSecondary,
                fontSize = 30.sp,
                fontWeight = FontWeight.Bold,
                fontFamily = TitilliumFontFamily
            )
        }
    }
}

// ---------------------------------------------------------------------------
// v1.32 — the in-game pause menu, Eden-style: back / PS button open it;
// Resume, Logs, pad controls and Exit live here. The game surface stays
// mounted underneath (renderer keeps running — Eden semantics).
// ---------------------------------------------------------------------------
@Composable
private fun EmuInGameMenu(
    gameName: String,
    padShown: Boolean,
    sessionReport: String?,
    fpsText: String = "",
    frametimeText: String = "",
    onResume: () -> Unit,
    onTogglePad: () -> Unit,
    onEditPad: () -> Unit,
    onOpenLogs: () -> Unit,
    onExit: () -> Unit
) {
    val c = px5Colors()
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(c.scrim)
            .pointerInput(Unit) {
                detectTapGestures { onResume() }
            },
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .width(300.dp)
                .clip(RoundedCornerShape(20.dp))
                .background(c.sheet.copy(alpha = 0.98f))
                .border(1.dp, c.hairline, RoundedCornerShape(20.dp))
                .padding(vertical = 8.dp)
        ) {
            Column(
                modifier = Modifier.padding(horizontal = 18.dp, vertical = 10.dp)
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        "PSX5",
                        color = PS5AccentBlue, fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = TitilliumFontFamily,
                        letterSpacing = 2.sp
                    )
                    Spacer(Modifier.weight(1f))
                    Text(
                        "Paused",
                        color = c.textSecondary, fontSize = 11.sp,
                        fontFamily = TitilliumFontFamily
                    )
                }
                Spacer(Modifier.height(2.dp))
                Text(
                    gameName,
                    color = c.text, fontSize = 16.sp,
                    fontWeight = FontWeight.SemiBold,
                    fontFamily = TitilliumFontFamily,
                    maxLines = 1
                )
                // v1.36 — the counters that used to float over the game
                // surface. They belong in this menu: the player opens the
                // menu to READ diagnostics, not mid-combat.
                if (fpsText.isNotEmpty() || frametimeText.isNotEmpty()) {
                    Spacer(Modifier.height(8.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        if (fpsText.isNotEmpty()) StatusChip(fpsText, Color(0xFF69F0AE))
                        if (frametimeText.isNotEmpty()) StatusChip(frametimeText, Color(0xFF7DD3FC))
                    }
                }
                if (sessionReport != null) {
                    Spacer(Modifier.height(6.dp))
                    Text(
                        "Session report available — see Logs",
                        color = Color(0xFFE2C74B), fontSize = 11.sp,
                        fontFamily = TitilliumFontFamily
                    )
                }
            }
            Spacer(Modifier.height(4.dp))
            EmuMenuRow(Icons.Default.PlayArrow, "Resume", c.text, true, onResume)
            EmuMenuRow(Icons.Default.Description, "Diagnostics & logs", c.text, true, onOpenLogs)
            EmuMenuRow(Icons.Default.Edit, "Edit pad layout", c.text, padShown, onEditPad)
            EmuMenuRow(
                Icons.Default.SportsEsports,
                if (padShown) "Hide touch pad" else "Show touch pad",
                c.text, true, onTogglePad
            )
            Box(
                modifier = Modifier
                    .padding(horizontal = 14.dp, vertical = 6.dp)
                    .fillMaxWidth()
                    .height(1.dp)
                    .background(c.hairline)
            )
            EmuMenuRow(Icons.Default.ExitToApp, "Exit to library", c.danger, true, onExit)
        }
    }
}

@Composable
private fun EmuMenuRow(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    tint: Color,
    enabled: Boolean,
    onClick: () -> Unit
) {
    val c = px5Colors()
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .fillMaxWidth()
            .alpha(if (enabled) 1f else 0.4f)
            .clickable(enabled = enabled, onClick = onClick)
            .padding(horizontal = 14.dp, vertical = 11.dp)
    ) {
        Box(
            modifier = Modifier
                .size(34.dp)
                .clip(RoundedCornerShape(10.dp))
                .background(c.control),
            contentAlignment = Alignment.Center
        ) {
            Icon(icon, contentDescription = label,
                 tint = tint, modifier = Modifier.size(17.dp))
        }
        Spacer(Modifier.width(12.dp))
        Text(
            label,
            color = tint, fontSize = 14.sp,
            fontFamily = TitilliumFontFamily
        )
    }
}

/** Process-wide marker: the boot diagnostics are one-shot per process. */
private object EmuScreenBoot {
    @Volatile var bootDiagnosticsDone: Boolean = false

    /**
     * v1.32: per-process guard for the contained execution probe. Holds
     * the eboot target already probed this process — re-entering the
     * same game skips the 8 s probe (evidence already on disk) and goes
     * straight to the mapped image.
     */
    @Volatile var execProbedPath: String? = null

    /**
     * v1.23: per-process guard for the auto conformance decision. Holds the
     * versionCode already decided this process (Int.MIN_VALUE = undecided).
     * The PERSISTED flag (prefs cpuGateInprocVc) is the once-per-build
     * guard; this one only keeps re-entries in the same process from
     * re-emitting the decision line.
     */
    @Volatile var conformanceDecidedVc: Int = Int.MIN_VALUE
}

// ---------------------------------------------------------------------------
// Honest eboot.bin probe for the boot-report line.
//
// Root cause (device log 2026-08-29): a plain java.io.File listing on API
// 30+ scoped storage returns null for SAF-imported game folders, and the
// old probe then printed "eboot=ABSENT" for a folder that CONTAINS
// eboot.bin. The probe is tiered — and since v1.27 every negative verdict
// CARRIES ITS ENUMERATION EVIDENCE, because an evidence-free ABSENT cannot
// be told apart from a silently-failed listing (the 2026-08-30/31 device
// sessions vs the user's own folder listing — eboot.bin visibly present —
// ended in exactly that unresolvable contradiction):
//   1. Direct java.io listing (definitive when storage is readable).
//   2. Bounded recursion through the folder tree. ABSENT is claimed only
//      with the walk counts attached (dirs walked / entries seen /
//      unreadable dirs).
//   3. SAF listing through persisted tree permissions — v1.27: bounded
//      RECURSIVE walk (was direct-children-only, which claimed ABSENT for
//      nested dumps it never enumerated), including content: game paths.
//   4. "unknown(...)" — when nothing actually enumerated, we never
//      fabricate ABSENT, and the load button stays enabled.
//
//   v1.28 — the decisive lesson from the vc28 session: a java.io listing
//   that SUCCEEDS under scoped storage can still be a FILTERED view (FUSE
//   readdir hides other apps' non-media files — the session walked 9 dirs
//   and saw 8 entries while the user's file manager showed 23 at the root
//   alone). So a negative java.io verdict is TENTATIVE and is cross-checked
//   through SAF (which sees the real directory contents) before ABSENT is
//   claimed; ABSENT requires BOTH available views to have enumerated and
//   missed. A java.io-only miss reports "unverified-empty" and never gates
//   the load button.
// ---------------------------------------------------------------------------
private fun probeEbootStatus(path: String, context: Context): String = try {
    if (path.startsWith("content:")) {
        safWalkVerdict(safSearchFromPath(path, context))
                ?: "unknown(no-list-permission)"
    } else {
        val bootDir = java.io.File(path)
        when {
            bootDir.isFile -> "file(${bootDir.length()}B)"
            bootDir.isDirectory -> {
                val listed = bootDir.listFiles()
                val direct = listed?.firstOrNull {
                    it.isFile && it.name.equals("eboot.bin", true)
                }
                if (direct != null) {
                    "present(${direct.length()}B)"
                } else {
                    // v1.28: a java.io listing that "succeeds" under scoped
                    // storage can still be a FILTERED view (FUSE readdir
                    // hides other apps' non-media files — the vc28 session
                    // walked 9 dirs and saw 8 entries while the user's file
                    // manager showed 23 at the root alone). A negative
                    // java.io verdict is therefore TENTATIVE: cross-check
                    // through SAF, which sees the real directory contents,
                    // before any ABSENT is claimed.
                    val out = listed?.let { com.px5.emulator.EbootLocator.search(bootDir) }
                    val saf = safWalkVerdict(safSearchFromPath(path, context))
                    val outFound = out?.found
                    when {
                        outFound != null ->
                            "present(${outFound.file.length()}B,${outFound.relPath})"
                        saf != null && saf.startsWith("present") -> saf
                        out != null && saf != null -> {
                            // both views enumerated, both missed — the honest ABSENT
                            val inner = saf.removePrefix("ABSENT(").removeSuffix(")")
                            "ABSENT($inner; ${ioWalkEvidence(out)})"
                        }
                        out != null ->
                            // java.io missed AND SAF cannot cross-check: a
                            // filtered view cannot prove absence — the load
                            // button stays enabled and the report names the
                            // gap (never a bare ABSENT).
                            "unverified-empty (java.io ${ioWalkEvidence(out)}; " +
                                    "SAF cross-check unavailable: no covering persisted tree)"
                        else -> saf ?: "unknown(no-list-permission)"
                    }
                }
            }
            else -> safWalkVerdict(safSearchFromPath(path, context))
                    ?: "path-missing"
        }
    }
} catch (_: Throwable) {
    "unknown(probe-error)"
}

private fun ioWalkEvidence(out: com.px5.emulator.EbootLocator.Outcome): String =
    "java.io walked ${out.stats.dirsWalked} dirs, ${out.stats.entriesSeen} entries, " +
    "${out.stats.unreadableDirs} unreadable"

private class SafFound(val uri: android.net.Uri, val relPath: String, val size: Long)

private class SafWalk(val found: SafFound?,
                      val dirsWalked: Int,
                      val dirsEnumerated: Int,
                      val entriesSeen: Int)

/** Map a negative SAF walk onto the report vocabulary; null = nothing was
 *  actually enumerated, so the caller must NOT claim ABSENT. */
private fun safWalkVerdict(w: SafWalk?): String? = when {
    w == null -> null
    w.found != null ->
        if (w.found.size >= 0) "present(${w.found.size}B,${w.found.relPath},SAF)"
        else "present(${w.found.relPath},SAF)"
    w.dirsEnumerated > 0 ->
        "ABSENT(SAF walked ${w.dirsWalked} dirs, ${w.dirsEnumerated} enumerated, " +
        "${w.entriesSeen} entries)"
    else -> null
}

/** (treeUri, startDocId) pairs of persisted trees that cover `path`.
 *  content: paths search their own tree; real paths are mapped under the
 *  "primary" persisted trees by prefix. */
private fun safTargetsFor(path: String, context: Context):
        List<Pair<android.net.Uri, String>> {
    if (path.startsWith("content:")) {
        return runCatching {
            val uri = android.net.Uri.parse(path)
            DocumentsContract.getTreeDocumentId(uri) // throws when no tree segment
            val startDoc = if (DocumentsContract.isTreeUri(uri))
                DocumentsContract.getTreeDocumentId(uri)
            else DocumentsContract.getDocumentId(uri)
            listOf(Pair(uri, startDoc))
        }.getOrDefault(emptyList())
    }
    val out = ArrayList<Pair<android.net.Uri, String>>()
    for (perm in context.contentResolver.persistedUriPermissions) {
        if (!perm.isReadPermission) continue
        val treeUri = perm.uri
        if (treeUri.scheme != "content") continue
        val mapped: Pair<android.net.Uri, String>? = runCatching {
            val treeDocId = DocumentsContract.getTreeDocumentId(treeUri)
            val root = treeDocId.substringBefore(':')
            if (root != "primary") return@runCatching null
            val rootRest = treeDocId.substringAfter(':', "")
            val rootPath = if (rootRest.isBlank()) "/storage/emulated/0"
                           else "/storage/emulated/0/$rootRest"
            val normPath = path.trimEnd('/')
            val normRoot = rootPath.trimEnd('/')
            if (!(normPath == normRoot || normPath.startsWith("$normRoot/"))) {
                return@runCatching null
            }
            val rel = if (normPath.length > normRoot.length)
                normPath.substring(normRoot.length + 1) else ""
            val targetDocId = if (rel.isBlank()) treeDocId else "$treeDocId/$rel"
            Pair(treeUri, targetDocId)
        }.getOrNull()
        if (mapped != null) out.add(mapped)
    }
    return out
}

/** v1.27 — bounded recursive eboot.bin walk over SAF, mirroring the
 *  java.io EbootLocator contract (depth 4 / 96 dirs, decrypted-first,
 *  alphabetical, deterministic). */
private fun safSearchEboot(context: Context, treeUri: android.net.Uri,
                           rootDocId: String): SafWalk {
    data class Entry(val docId: String, val depth: Int, val rel: String)
    data class Row(val name: String, val isDir: Boolean,
                   val docId: String, val size: Long)
    data class DirRow(val name: String, val docId: String,
                      val depth: Int, val rel: String)
    val queue = ArrayDeque<Entry>()
    queue.add(Entry(rootDocId, 0, ""))
    var walked = 0
    var enumerated = 0
    var seen = 0
    while (queue.isNotEmpty()) {
        val cur = queue.removeFirst()
        walked++
        val rows: List<Row>? = runCatching {
            val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                    treeUri, cur.docId)
            val out = ArrayList<Row>()
            context.contentResolver.query(
                childrenUri,
                arrayOf(
                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE,
                    DocumentsContract.Document.COLUMN_SIZE),
                null, null, null)?.use { c ->
                while (c.moveToNext()) {
                    val docId = c.getString(0) ?: continue
                    val name = c.getString(1) ?: continue
                    val mime = c.getString(2) ?: ""
                    val size = if (c.isNull(3)) -1L else c.getLong(3)
                    out.add(Row(name,
                                mime == DocumentsContract.Document.MIME_TYPE_DIR,
                                docId, size))
                }
            }
            out
        }.getOrNull()
        if (rows == null) continue
        enumerated++
        seen += rows.size
        val subDirs = ArrayList<DirRow>()
        for (r in rows) {
            val childRel = if (cur.rel.isEmpty()) r.name else "${cur.rel}/${r.name}"
            if (!r.isDir && r.name.equals("eboot.bin", ignoreCase = true)) {
                return SafWalk(SafFound(
                        DocumentsContract.buildDocumentUriUsingTree(treeUri, r.docId),
                        childRel, r.size), walked, enumerated, seen)
            }
            if (r.isDir) subDirs.add(DirRow(r.name, r.docId, cur.depth + 1, childRel))
        }
        if (cur.depth + 1 > com.px5.emulator.EbootLocator.MAX_DEPTH ||
                walked >= com.px5.emulator.EbootLocator.MAX_DIRS) continue
        subDirs.sortBy { it.name.lowercase() }
        subDirs.sortByDescending { it.name.equals("decrypted", ignoreCase = true) }
        for (d in subDirs) queue.add(Entry(d.docId, d.depth, d.rel))
    }
    return SafWalk(null, walked, enumerated, seen)
}

private fun safSearchFromPath(path: String, context: Context): SafWalk? {
    val targets = safTargetsFor(path, context)
    if (targets.isEmpty()) return null
    var walked = 0
    var enumerated = 0
    var seen = 0
    var any = false
    for ((treeUri, docId) in targets) {
        val w = runCatching { safSearchEboot(context, treeUri, docId) }.getOrNull()
                ?: continue
        any = true
        if (w.found != null) return w
        walked += w.dirsWalked
        enumerated += w.dirsEnumerated
        seen += w.entriesSeen
    }
    return if (any) SafWalk(null, walked, enumerated, seen) else null
}

/** v1.27 — resolve a loadable eboot target when java.io cannot read the
 *  game folder: find the document over SAF, copy the bytes into the app
 *  cache, and hand the native loader the copy's real path. */
private fun safCopyTarget(path: String, context: Context): Pair<java.io.File, String>? =
    runCatching {
        val w = safSearchFromPath(path, context) ?: return@runCatching null
        val found = w.found ?: return@runCatching null
        val ins = context.contentResolver.openInputStream(found.uri)
                  ?: return@runCatching null
        val dest = java.io.File(context.cacheDir, "saf_eboot.bin")
        ins.use { s -> dest.outputStream().use { s.copyTo(it, 64 * 1024) } }
        if (dest.length() == 0L) {
            dest.delete()
            return@runCatching null
        }
        Pair(dest, "route=saf-copy rel=${found.relPath} bytes=${dest.length()}")
    }.getOrNull()
