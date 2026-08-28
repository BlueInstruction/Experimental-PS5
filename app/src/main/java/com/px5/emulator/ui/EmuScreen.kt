package com.px5.emulator.ui

import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.px5.emulator.core.FexCoreWrapper
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

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
    var gpuProof by remember { mutableStateOf<String?>(null) }
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
        }
    }

    val isStubAbi = fexCoreWrapper?.nativeGetArchitectureSummary()
        ?.contains("UI-smoke ABI") == true

    // One-shot honest GPU proof (skipped on the UI-smoke x86_64 ABI).
    LaunchedEffect(Unit) {
        if (!isStubAbi) {
            launch(Dispatchers.Default) {
                gpuProof = try {
                    fexCoreWrapper?.nativeRunGpuProof() ?: "wrapper missing"
                } catch (t: Throwable) { "FAIL | ${t.message}" }
            }
        }
    }

    // Live HUD polling.
    LaunchedEffect(Unit) {
        while (true) {
            try {
                renderStats = fexCoreWrapper?.nativeGetRenderStats()
                    ?: "no engine"
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
                         color = PS5AccentGlow, fontSize = 12.sp,
                         fontFamily = TitilliumFontFamily, maxLines = 1)
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
            gpuProof?.let { p ->
                Text(
                    text = "GPU proof: $p",
                    fontSize = 11.sp,
                    color = if (p.startsWith("PASS")) Color(0xFF7DD3FC)
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
                color = PS5TextSecondary
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
                            loadResult = if (target.isBlank()) {
                                "LOAD FAILED: no eboot.bin in folder"
                            } else {
                                try {
                                    val ok = fexCoreWrapper.nativeLoadElf(target)
                                    if (ok) "LOADED: $target mapped into guest window"
                                    else "LOAD FAILED: loader rejected $target (see logcat)"
                                } catch (t: Throwable) {
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
                    Text("Attempt ELF load (experimental)", fontSize = 12.sp)
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

            // ---------------- interactive pad ---------------------------------
            TouchPadGrid(
                enabled = !isStubAbi,
                onPress = { bit, down ->
                    fexCoreWrapper?.nativeSetButtonState(bit, down)
                },
                onDismissPress = onBackClick
            )
            Spacer(Modifier.height(10.dp))
        }
    }
}

/**
 * Compact interactive pad. Every press/release goes through
 * FexCoreWrapper.nativeSetButtonState into PX5::InputManager atomics; the
 * HUD's "input:" line above proves events actually reached C++.
 */
@Composable
private fun TouchPadGrid(
    enabled: Boolean,
    onPress: (Int, Boolean) -> Unit,
    onDismissPress: () -> Unit
) {
    // D-pad cross layout
    Row(verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.fillMaxWidth()) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            PadKey("\u25B2", Color.White, enabled, onPress, FexCoreWrapper.PAD_DPAD_UP)
            Spacer(Modifier.height(4.dp))
            Row {
                PadKey("\u25C0", Color.White, enabled, onPress, FexCoreWrapper.PAD_DPAD_LEFT)
                Spacer(Modifier.width(4.dp))
                Box(Modifier.size(46.dp))      // keeps cross spacing
                Spacer(Modifier.width(4.dp))
                PadKey("\u25B6", Color.White, enabled, onPress, FexCoreWrapper.PAD_DPAD_RIGHT)
            }
            Spacer(Modifier.height(4.dp))
            PadKey("\u25BC", Color.White, enabled, onPress, FexCoreWrapper.PAD_DPAD_DOWN)
        }

        Spacer(Modifier.weight(1f))

        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Row {
                PadKey("\u25A1", Color(0xFFEF7DBD), enabled, onPress, FexCoreWrapper.PAD_SQUARE)
                Spacer(Modifier.width(6.dp))
                PadKey("\u25B3", Color(0xFF4CD9A6), enabled, onPress, FexCoreWrapper.PAD_TRIANGLE)
            }
            Spacer(Modifier.height(6.dp))
            Row {
                PadKey("\u00D7", Color(0xFF7FB8FF), enabled, onPress, FexCoreWrapper.PAD_CROSS)
                Spacer(Modifier.width(6.dp))
                PadKey("\u25CB", Color(0xFFFF6B6B), enabled, onPress, FexCoreWrapper.PAD_CIRCLE)
            }
        }
    }
}

@Composable
private fun PadKey(
    label: String,
    tint: Color,
    enabled: Boolean,
    onPress: (Int, Boolean) -> Unit,
    bit: Int
) {
    var pressed by remember { mutableStateOf(false) }
    val shape = CircleShape

    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .size(46.dp)
            .clip(shape)
            .background(if (pressed) tint.copy(alpha = 0.35f)
                        else Color.White.copy(alpha = 0.06f))
            .border(1.5.dp, if (pressed) tint else tint.copy(alpha = 0.5f), shape)
            .then(if (enabled) {
                Modifier.pointerInput(bit) {
                    detectTapGestures(onPress = {
                        pressed = true
                        onPress(bit, true)
                        try { awaitRelease() } catch (_: Throwable) {}
                        pressed = false
                        onPress(bit, false)
                    })
                }
            } else Modifier)
    ) {
        Text(label, color = tint, fontSize = 14.sp, fontWeight = FontWeight.Bold)
    }
}
