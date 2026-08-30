package com.px5.emulator.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.core.FexCoreWrapper
import kotlin.math.max
import kotlin.math.roundToInt
import kotlin.math.sqrt

// ---------------------------------------------------------------------------
// PS5VirtualPad — DualSense-accurate on-screen controller, V2.
//
// Design language (matched to the reference layout the user approved):
//   * translucent black-glass buttons with hairline light borders,
//   * PS glyph shapes drawn as vectors (triangle/circle/cross/square),
//   * analog L2/R2 triggers, L3/R3 stick-click pills, PS home,
//     Create/Options pills, touchpad pill, joined d-pad cross,
//   * edit layer: drag any element; fractional layout persists.
//
// Every press lands in the native InputManager atomics over JNI:
// buttons -> nativeSetButtonState, sticks -> nativeSetLeft/RightStick,
// L2/R2 -> nativeSetTriggers, touchpad -> nativeSetTouchpad.
// ---------------------------------------------------------------------------

/** DualSense face-button colors (screen-tuned, same hues as the real pad). */
val PS_TRIANGLE = Color(0xFF3FD9A3)
val PS_CIRCLE = Color(0xFFFF6B6B)
val PS_CROSS = Color(0xFF8AB8FF)
val PS_SQUARE = Color(0xFFF07BD0)

private val GLASS_FILL = Color(0xFF0F1218)
private val GLASS_EDGE = Color.White.copy(alpha = 0.28f)
private val GLASS_PRESSED = Color(0xFF2E8CFF)

/** Face-button glyph shapes, stroke-drawn (no fonts, no bitmaps). */
private enum class PsGlyph { TRIANGLE, CIRCLE, CROSS, SQUARE }

/** One placed pad element. fx/fy = fractional CENTER inside the stage. */
data class PadPlacement(val name: String, val fx: Float, val fy: Float)

object PadLayout {
    /** Defaults mirror a DualSense seen from the player's point of view. */
    fun defaults(): Map<String, PadPlacement> = mapOf(
        "L2" to PadPlacement("L2", 0.048f, 0.10f),
        "L1" to PadPlacement("L1", 0.048f, 0.25f),
        "R2" to PadPlacement("R2", 0.952f, 0.10f),
        "R1" to PadPlacement("R1", 0.952f, 0.25f),
        "L3" to PadPlacement("L3", 0.030f, 0.47f),
        "R3" to PadPlacement("R3", 0.970f, 0.47f),
        "DPAD" to PadPlacement("DPAD", 0.115f, 0.72f),
        "LS" to PadPlacement("LS", 0.295f, 0.79f),
        "RS" to PadPlacement("RS", 0.705f, 0.79f),
        "FACE" to PadPlacement("FACE", 0.885f, 0.72f),
        "CREATE" to PadPlacement("CREATE", 0.437f, 0.91f),
        "PS" to PadPlacement("PS", 0.500f, 0.91f),
        "OPTIONS" to PadPlacement("OPTIONS", 0.563f, 0.91f),
        "TP" to PadPlacement("TP", 0.500f, 0.78f)
    )

    fun decode(json: String): Map<String, PadPlacement> {
        val base = defaults()
        if (json.isBlank()) return base
        return try {
            val obj = org.json.JSONObject(json)
            val out = base.toMutableMap()
            for (key in base.keys) {
                if (obj.has(key)) {
                    val arr = obj.getJSONArray(key)
                    out[key] = PadPlacement(
                        key,
                        arr.getDouble(0).toFloat().coerceIn(0.02f, 0.98f),
                        arr.getDouble(1).toFloat().coerceIn(0.05f, 0.96f)
                    )
                }
            }
            out
        } catch (_: Throwable) {
            defaults()
        }
    }

    fun encode(layout: Map<String, PadPlacement>): String = try {
        val obj = org.json.JSONObject()
        for ((k, p) in layout) obj.put(k, org.json.JSONArray(listOf(p.fx, p.fy)))
        obj.toString()
    } catch (_: Throwable) {
        ""
    }
}

/**
 * DualSensePadV2 — absolutely positioned glass controller over the game
 * surface. `editing = true` turns on the drag layer; movement persists
 * through [PadLayout]. All input callbacks must be no-op safe when
 * `enabled` is false (UI-smoke ABI).
 */
@Composable
fun DualSensePadV2(
    enabled: Boolean,
    opacity01: Float,
    scale01: Float,
    editing: Boolean,
    layout: Map<String, PadPlacement>,
    onLayoutChange: (Map<String, PadPlacement>) -> Unit,
    onButton: (Int, Boolean) -> Unit,
    onLeftStick: (Float, Float) -> Unit,
    onRightStick: (Float, Float) -> Unit,
    onTriggers: (Float, Float) -> Unit,
    onTouchpad: (Boolean) -> Unit
) {
    var stageW by remember { mutableIntStateOf(1) }
    var stageH by remember { mutableIntStateOf(1) }
    val l2 = remember { mutableFloatStateOf(0f) }
    val r2 = remember { mutableFloatStateOf(0f) }

    fun move(name: String, fx: Float, fy: Float) {
        onLayoutChange(layout + (name to PadPlacement(name, fx, fy)))
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .alpha(opacity01)
            .onGloballyPositioned { stageW = it.size.width; stageH = it.size.height }
    ) {
        // --- shoulders & triggers -------------------------------------------
        Placed(layout["L2"]!!, stageW, stageH, editing, ::move) {
            GlassKey(68.dp * scale01, 30.dp * scale01, "L2", enabled, editing) { down ->
                l2.floatValue = if (down) 1f else 0f
                onTriggers(l2.floatValue, r2.floatValue)
            }
        }
        Placed(layout["L1"]!!, stageW, stageH, editing, ::move) {
            GlassKey(64.dp * scale01, 27.dp * scale01, "L1", enabled, editing) { down ->
                onButton(FexCoreWrapper.PAD_L1, down)
            }
        }
        Placed(layout["R2"]!!, stageW, stageH, editing, ::move) {
            GlassKey(68.dp * scale01, 30.dp * scale01, "R2", enabled, editing) { down ->
                r2.floatValue = if (down) 1f else 0f
                onTriggers(l2.floatValue, r2.floatValue)
            }
        }
        Placed(layout["R1"]!!, stageW, stageH, editing, ::move) {
            GlassKey(64.dp * scale01, 27.dp * scale01, "R1", enabled, editing) { down ->
                onButton(FexCoreWrapper.PAD_R1, down)
            }
        }
        // --- stick clicks -----------------------------------------------------
        Placed(layout["L3"]!!, stageW, stageH, editing, ::move) {
            PillKey(38.dp * scale01, 23.dp * scale01, "L3", enabled, editing, labelSize = 10.sp) { down ->
                onButton(FexCoreWrapper.PAD_L3, down)
            }
        }
        Placed(layout["R3"]!!, stageW, stageH, editing, ::move) {
            PillKey(38.dp * scale01, 23.dp * scale01, "R3", enabled, editing, labelSize = 10.sp) { down ->
                onButton(FexCoreWrapper.PAD_R3, down)
            }
        }
        // --- d-pad / sticks / face -------------------------------------------
        Placed(layout["DPAD"]!!, stageW, stageH, editing, ::move) {
            DPadGlass(118.dp * scale01, 40.dp * scale01, enabled, editing, onButton)
        }
        Placed(layout["LS"]!!, stageW, stageH, editing, ::move) {
            StickGlass(92.dp * scale01, 40.dp * scale01, enabled, editing) { x, y ->
                onLeftStick(x, y)
            }
        }
        Placed(layout["RS"]!!, stageW, stageH, editing, ::move) {
            StickGlass(92.dp * scale01, 40.dp * scale01, enabled, editing) { x, y ->
                onRightStick(x, y)
            }
        }
        Placed(layout["FACE"]!!, stageW, stageH, editing, ::move) {
            FaceDiamondGlass(128.dp * scale01, 46.dp * scale01, enabled, editing, onButton)
        }
        // --- center cluster ---------------------------------------------------
        Placed(layout["CREATE"]!!, stageW, stageH, editing, ::move) {
            PillKey(
                42.dp * scale01, 23.dp * scale01, "", enabled, editing,
                glyph = { CreateGlyph() }
            ) { down -> onButton(FexCoreWrapper.PAD_SHARE, down) }
        }
        Placed(layout["PS"]!!, stageW, stageH, editing, ::move) {
            PillKey(
                42.dp * scale01, 26.dp * scale01, "PS", enabled, editing,
                labelSize = 10.sp
            ) { down -> onButton(FexCoreWrapper.PAD_PS_HOME, down) }
        }
        Placed(layout["OPTIONS"]!!, stageW, stageH, editing, ::move) {
            PillKey(
                42.dp * scale01, 23.dp * scale01, "", enabled, editing,
                glyph = { OptionsGlyph() }
            ) { down -> onButton(FexCoreWrapper.PAD_OPTIONS, down) }
        }
        Placed(layout["TP"]!!, stageW, stageH, editing, ::move) {
            PillKey(
                60.dp * scale01, 20.dp * scale01, "", enabled, editing,
                glyph = { TouchpadGlyph() }
            ) { down -> onTouchpad(down) }
        }
    }
}

/**
 * Positions one pad element so its CENTER sits at (fx, fy) of the stage
 * and — in edit mode — lets the user drag it anywhere (persisted).
 */
@Composable
private fun Placed(
    placement: PadPlacement,
    stageW: Int,
    stageH: Int,
    editing: Boolean,
    onMove: (String, Float, Float) -> Unit,
    content: @Composable () -> Unit
) {
    var w by remember { mutableIntStateOf(0) }
    var h by remember { mutableIntStateOf(0) }
    val cur by rememberUpdatedState(placement)
    val moveCb by rememberUpdatedState(onMove)
    Box(
        modifier = Modifier
            .offset {
                IntOffset(
                    (cur.fx * stageW).roundToInt() - w / 2,
                    (cur.fy * stageH).roundToInt() - h / 2
                )
            }
            .onGloballyPositioned { w = it.size.width; h = it.size.height }
            .then(
                if (editing) {
                    Modifier
                        .drawBehind {
                            val dash = PathEffect.dashPathEffect(floatArrayOf(14f, 10f))
                            drawRoundRect(
                                color = Color(0xFF2E8CFF).copy(alpha = 0.85f),
                                cornerRadius = CornerRadius(18f, 18f),
                                style = Stroke(width = 4f, pathEffect = dash)
                            )
                        }
                        .pointerInput(Unit) {
                            detectDragGestures(
                                onDrag = { change, amt ->
                                    change.consume()
                                    moveCb(
                                        cur.name,
                                        (cur.fx + amt.x / max(stageW, 1))
                                            .coerceIn(0.02f, 0.98f),
                                        (cur.fy + amt.y / max(stageH, 1))
                                            .coerceIn(0.05f, 0.96f)
                                    )
                                }
                            )
                        }
                } else Modifier
            )
    ) { content() }
}

// ---------------------------------------------------------------------------
// Glass elements
// ---------------------------------------------------------------------------

/** Shoulder/trigger bar (L1/R1/L2/R2). Glass rect, press -> callback. */
@Composable
private fun GlassKey(
    w: Dp,
    h: Dp,
    label: String,
    enabled: Boolean,
    editing: Boolean,
    onPress: (Boolean) -> Unit
) {
    var pressed by remember { mutableStateOf(false) }
    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .size(w, h)
            .clip(RoundedCornerShape(9.dp))
            .background(
                if (pressed) GLASS_PRESSED.copy(alpha = 0.50f)
                else GLASS_FILL.copy(alpha = 0.60f)
            )
            .border(
                1.5.dp,
                if (pressed) GLASS_PRESSED else GLASS_EDGE,
                RoundedCornerShape(9.dp)
            )
            .then(
                if (enabled && !editing) Modifier.pointerInput(label) {
                    detectTapGestures(onPress = {
                        pressed = true
                        onPress(true)
                        try { awaitRelease() } catch (_: Throwable) {}
                        pressed = false
                        onPress(false)
                    })
                } else Modifier
            )
    ) {
        Text(
            label,
            color = Color.White,
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            fontFamily = TitilliumFontFamily
        )
    }
}

/** Small glass pill (L3/R3, PS, Create/Options, touchpad). */
@Composable
private fun PillKey(
    w: Dp,
    h: Dp,
    label: String,
    enabled: Boolean,
    editing: Boolean,
    labelSize: androidx.compose.ui.unit.TextUnit = 11.sp,
    glyph: (@Composable () -> Unit)? = null,
    onPress: (Boolean) -> Unit
) {
    var pressed by remember { mutableStateOf(false) }
    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .size(w, h)
            .clip(RoundedCornerShape(h / 2))
            .background(
                if (pressed) GLASS_PRESSED.copy(alpha = 0.50f)
                else GLASS_FILL.copy(alpha = 0.60f)
            )
            .border(
                1.dp,
                if (pressed) GLASS_PRESSED else GLASS_EDGE,
                RoundedCornerShape(h / 2)
            )
            .then(
                if (enabled && !editing) Modifier.pointerInput(label, w) {
                    detectTapGestures(onPress = {
                        pressed = true
                        onPress(true)
                        try { awaitRelease() } catch (_: Throwable) {}
                        pressed = false
                        onPress(false)
                    })
                } else Modifier
            )
    ) {
        if (glyph != null) glyph()
        else Text(
            label,
            color = Color.White.copy(alpha = 0.92f),
            fontSize = labelSize,
            fontWeight = FontWeight.Bold,
            fontFamily = TitilliumFontFamily
        )
    }
}

/**
 * Joined d-pad cross: one rounded plus-shape drawn on Canvas with the
 * pressed arm highlighted; four transparent hit zones above it keep
 * multi-touch (hold Up while dragging the stick) working.
 */
@Composable
private fun DPadGlass(
    box: Dp,
    lobe: Dp,
    enabled: Boolean,
    editing: Boolean,
    onButton: (Int, Boolean) -> Unit
) {
    var pressed by remember { mutableStateOf(0) }   // 1 up 2 down 3 left 4 right
    Box(
        modifier = Modifier.size(box, box),
        contentAlignment = Alignment.Center
    ) {
        Canvas(modifier = Modifier.size(box, box)) {
            val b = size.width
            val l = lobe.toPx()
            val r = CornerRadius(l * 0.30f, l * 0.30f)
            val fill = GLASS_FILL.copy(alpha = 0.60f)
            val edge = GLASS_EDGE
            val hi = GLASS_PRESSED.copy(alpha = 0.50f)
            val hiEdge = GLASS_PRESSED
            fun arm(dir: Int): Rect {
                return when (dir) {
                    1 -> Rect(Offset(b / 2 - l / 2, 0f), Size(l, b / 2 - l / 2 + l / 2))
                    2 -> Rect(Offset(b / 2 - l / 2, b / 2 - l / 2), Size(l, b / 2))
                    3 -> Rect(Offset(0f, b / 2 - l / 2), Size(b / 2 - l / 2 + l / 2, l))
                    else -> Rect(Offset(b / 2 - l / 2, b / 2 - l / 2), Size(b / 2, l))
                }
            }
            // cross body: vertical + horizontal bars
            drawRoundRect(fill, topLeft = Offset(b / 2 - l / 2, 0f),
                size = Size(l, b), cornerRadius = r)
            drawRoundRect(fill, topLeft = Offset(0f, b / 2 - l / 2),
                size = Size(b, l), cornerRadius = r)
            drawRoundRect(edge, topLeft = Offset(b / 2 - l / 2, 0f),
                size = Size(l, b), cornerRadius = r, style = Stroke(3f))
            drawRoundRect(edge, topLeft = Offset(0f, b / 2 - l / 2),
                size = Size(b, l), cornerRadius = r, style = Stroke(3f))
            // pressed arm highlight
            if (pressed in 1..4) {
                drawRoundRect(hi, topLeft = arm(pressed).topLeft,
                    size = arm(pressed).size, cornerRadius = r)
                drawRoundRect(hiEdge, topLeft = arm(pressed).topLeft,
                    size = arm(pressed).size, cornerRadius = r, style = Stroke(3f))
            }
        }
        // hit zones (transparent, per-arm pointerInput = multi-touch)
        val armSize = lobe
        @Composable fun zone(dir: Int, align: Alignment, bit: Int) {
            Box(
                contentAlignment = Alignment.Center,
                modifier = Modifier
                    .align(align)
                    .size(
                        if (dir <= 2) armSize else box / 2,
                        if (dir <= 2) box / 2 else armSize
                    )
                    .then(
                        if (enabled && !editing) Modifier.pointerInput(bit) {
                            detectTapGestures(onPress = {
                                pressed = dir
                                onButton(bit, true)
                                try { awaitRelease() } catch (_: Throwable) {}
                                pressed = 0
                                onButton(bit, false)
                            })
                        } else Modifier
                    )
            ) {}
        }
        zone(1, Alignment.TopCenter, FexCoreWrapper.PAD_DPAD_UP)
        zone(2, Alignment.BottomCenter, FexCoreWrapper.PAD_DPAD_DOWN)
        zone(3, Alignment.CenterStart, FexCoreWrapper.PAD_DPAD_LEFT)
        zone(4, Alignment.CenterEnd, FexCoreWrapper.PAD_DPAD_RIGHT)
    }
}

/**
 * Analog stick: glass ring + sliding knob. Radial dead zone 12% with
 * rescale, clamp to the base ring, spring back to center on release.
 */
@Composable
private fun StickGlass(
    base: Dp,
    knob: Dp,
    enabled: Boolean,
    editing: Boolean,
    onMove: (Float, Float) -> Unit
) {
    var thumb by remember { mutableStateOf(Offset.Zero) }
    val density = LocalDensity.current
    val maxPx = with(density) { (base - knob).toPx() / 2f }

    fun emit(v: Offset) {
        val nx = v.x / maxPx
        val ny = v.y / maxPx
        val d = sqrt(nx * nx + ny * ny)
        if (d <= 0.12f) onMove(0f, 0f)
        else {
            val scale = ((d - 0.12f) / 0.88f) / d
            onMove(nx * scale, ny * scale)
        }
    }

    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .size(base)
            .then(
                if (enabled && !editing) Modifier.pointerInput(Unit) {
                    detectDragGestures(
                        onDrag = { change, dragAmount ->
                            change.consume()
                            val v = thumb + dragAmount
                            val d = sqrt(v.x * v.x + v.y * v.y)
                            thumb = if (d > maxPx && d > 0f) v * (maxPx / d) else v
                            emit(thumb)
                        },
                        onDragEnd = { thumb = Offset.Zero; onMove(0f, 0f) },
                        onDragCancel = { thumb = Offset.Zero; onMove(0f, 0f) }
                    )
                } else Modifier
            )
    ) {
        Canvas(modifier = Modifier.size(base)) {
            drawCircle(GLASS_FILL.copy(alpha = 0.55f))
            drawCircle(GLASS_EDGE, style = Stroke(3f))
            drawCircle(Color.White.copy(alpha = 0.06f), radius = size.minDimension * 0.30f)
        }
        Box(
            modifier = Modifier
                .offset { IntOffset(thumb.x.roundToInt(), thumb.y.roundToInt()) }
                .size(knob)
                .clip(CircleShape)
                .background(Color(0xFF232833).copy(alpha = 0.92f))
                .border(1.dp, Color.White.copy(alpha = 0.45f), CircleShape)
        )
    }
}

/** Face diamond: vector PS glyphs in real pad colors on glass circles. */
@Composable
private fun FaceDiamondGlass(
    box: Dp,
    key: Dp,
    enabled: Boolean,
    editing: Boolean,
    onButton: (Int, Boolean) -> Unit
) {
    Box(
        modifier = Modifier.size(box, box),
        contentAlignment = Alignment.Center
    ) {
        Box(Modifier.align(Alignment.TopCenter)) {
            FaceKey(key, PsGlyph.TRIANGLE, PS_TRIANGLE, enabled, editing) { d ->
                onButton(FexCoreWrapper.PAD_TRIANGLE, d)
            }
        }
        Box(Modifier.align(Alignment.BottomCenter)) {
            FaceKey(key, PsGlyph.CROSS, PS_CROSS, enabled, editing) { d ->
                onButton(FexCoreWrapper.PAD_CROSS, d)
            }
        }
        Box(Modifier.align(Alignment.CenterStart)) {
            FaceKey(key, PsGlyph.SQUARE, PS_SQUARE, enabled, editing) { d ->
                onButton(FexCoreWrapper.PAD_SQUARE, d)
            }
        }
        Box(Modifier.align(Alignment.CenterEnd)) {
            FaceKey(key, PsGlyph.CIRCLE, PS_CIRCLE, enabled, editing) { d ->
                onButton(FexCoreWrapper.PAD_CIRCLE, d)
            }
        }
    }
}

@Composable
private fun FaceKey(
    size: Dp,
    kind: PsGlyph,
    tint: Color,
    enabled: Boolean,
    editing: Boolean,
    onPress: (Boolean) -> Unit
) {
    var pressed by remember { mutableStateOf(false) }
    val density = LocalDensity.current
    val strokePx = with(density) { 2.4.dp.toPx() }
    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .size(size)
            .clip(CircleShape)
            .background(
                if (pressed) tint.copy(alpha = 0.30f)
                else GLASS_FILL.copy(alpha = 0.60f)
            )
            .border(
                1.dp,
                if (pressed) tint else GLASS_EDGE,
                CircleShape
            )
            .then(
                if (enabled && !editing) Modifier.pointerInput(tint) {
                    detectTapGestures(onPress = {
                        pressed = true
                        onPress(true)
                        try { awaitRelease() } catch (_: Throwable) {}
                        pressed = false
                        onPress(false)
                    })
                } else Modifier
            )
    ) {
        Canvas(modifier = Modifier.size(size * 0.52f)) {
            drawPsGlyph(kind, tint, stroke = strokePx)
        }
    }
}

// ---------------------------------------------------------------------------
// Vector PS glyphs (stroke-drawn, no fonts, no bitmaps)
// ---------------------------------------------------------------------------

private fun DrawScope.drawPsGlyph(kind: PsGlyph, tint: Color, stroke: Float) {
    val w = size.width
    val h = size.height
    val s = stroke
    when (kind) {
        PsGlyph.TRIANGLE -> {
            val p = Path().apply {
                moveTo(w / 2, s)
                lineTo(w - s, h - s)
                lineTo(s, h - s)
                close()
            }
            drawPath(p, tint, style = Stroke(s, cap = StrokeCap.Round))
        }
        PsGlyph.CIRCLE -> drawCircle(tint, radius = w / 2 - s,
            style = Stroke(s, cap = StrokeCap.Round))
        PsGlyph.CROSS -> {
            val k = w * 0.22f
            drawLine(tint, Offset(k, k), Offset(w - k, h - k), s, StrokeCap.Round)
            drawLine(tint, Offset(w - k, k), Offset(k, h - k), s, StrokeCap.Round)
        }
        PsGlyph.SQUARE -> drawRect(
            tint,
            topLeft = Offset(s, s),
            size = Size(w - 2 * s, h - 2 * s),
            style = Stroke(s, cap = StrokeCap.Round)
        )
    }
}

/** Create button glyph: three left-aligned lines + leading bar. */
@Composable
private fun CreateGlyph() {
    Canvas(modifier = Modifier.size(16.dp, 12.dp)) {
        val c = Color.White.copy(alpha = 0.85f)
        val w = size.width
        val h = size.height
        val t = 1.6f
        drawLine(c, Offset(w * 0.28f, h * 0.18f), Offset(w * 0.95f, h * 0.18f), t)
        drawLine(c, Offset(w * 0.28f, h * 0.5f), Offset(w * 0.95f, h * 0.5f), t)
        drawLine(c, Offset(w * 0.28f, h * 0.82f), Offset(w * 0.72f, h * 0.82f), t)
        drawLine(c, Offset(w * 0.10f, h * 0.10f), Offset(w * 0.10f, h * 0.90f), t)
    }
}

/** Options button glyph: three right-aligned lines + trailing bar. */
@Composable
private fun OptionsGlyph() {
    Canvas(modifier = Modifier.size(16.dp, 12.dp)) {
        val c = Color.White.copy(alpha = 0.85f)
        val w = size.width
        val h = size.height
        val t = 1.6f
        drawLine(c, Offset(w * 0.05f, h * 0.18f), Offset(w * 0.72f, h * 0.18f), t)
        drawLine(c, Offset(w * 0.05f, h * 0.5f), Offset(w * 0.72f, h * 0.5f), t)
        drawLine(c, Offset(w * 0.28f, h * 0.82f), Offset(w * 0.72f, h * 0.82f), t)
        drawLine(c, Offset(w * 0.90f, h * 0.10f), Offset(w * 0.90f, h * 0.90f), t)
    }
}

/** Touchpad glyph: rounded outline + the two corner slashes. */
@Composable
private fun TouchpadGlyph() {
    Canvas(modifier = Modifier.size(20.dp, 11.dp)) {
        val c = Color.White.copy(alpha = 0.80f)
        drawRoundRect(
            c,
            topLeft = Offset(1f, 1f),
            size = Size(size.width - 2f, size.height - 2f),
            cornerRadius = CornerRadius(4f, 4f),
            style = Stroke(1.6f)
        )
        drawLine(c, Offset(size.width * 0.22f, 2.5f), Offset(size.width * 0.12f, size.height - 2.5f), 1.4f)
        drawLine(c, Offset(size.width * 0.78f, 2.5f), Offset(size.width * 0.88f, size.height - 2.5f), 1.4f)
    }
}
