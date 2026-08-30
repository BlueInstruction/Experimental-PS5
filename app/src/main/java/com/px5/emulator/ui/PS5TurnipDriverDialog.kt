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
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.px5.emulator.DriverSlotStore
import com.px5.emulator.SoundManager
import com.px5.emulator.core.FexCoreWrapper
import com.px5.emulator.core.Px5Settings
import kotlinx.coroutines.launch
import java.io.File

/**
 * TurnipDriverSheet — manages REAL driver slots only.
 *
 * The previous dialog presented three selectable "profiles"
 * (Turnip Mesa 24.1.0 / 23.3.0 / system) that did not correspond to any
 * file on disk — selecting one changed a label, nothing else. This
 * version lists exactly what exists: the system ICD plus every imported
 * driver persisted in DriverSlotStore and registered in the native
 * GpuDriverManager. Selecting a slot really switches the loader mode;
 * removing slots clears the native registry and re-registers the
 * remaining ones; the summary line comes straight from the engine
 * (including the /proc/self/maps driver-verification state).
 */
@Composable
fun PS5TurnipDriverSheet(
    soundManager: SoundManager,
    fexCoreWrapper: FexCoreWrapper?,
    onImportCustomDriverClick: () -> Unit,
    onDismiss: () -> Unit
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var slots by remember { mutableStateOf(DriverSlotStore.load(context)) }
    var activeMode by remember { mutableStateOf(Px5Settings.driverMode.value) }
    var summary by remember { mutableStateOf("querying engine…") }
    var busy by remember { mutableStateOf(false) }

    suspend fun refreshSummary() {
        summary = runCatching { fexCoreWrapper?.nativeGetDriverManagerSummary() ?: "engine unavailable" }
            .getOrDefault("engine unavailable")
    }

    // v1.16: runs the REAL verification path (adrenotools load + maps
    // check) for the active slot and returns the fresh summary — the
    // manager stops answering "driverVerified=not-run" after a select or
    // an import.
    suspend fun verifyAndRefresh(slotIndex: Int) {
        summary = runCatching {
            fexCoreWrapper?.nativeVerifyDriverSlot(slotIndex) ?: "engine unavailable"
        }.getOrDefault("engine unavailable")
    }

    LaunchedEffect(Unit) { refreshSummary() }

    // v1.16 — the v1.15 session's second driver complaint: a freshly
    // imported slot did not appear until the user pressed "refresh driver
    // status", because the sheet captured DriverSlotStore once at
    // composition and the import finishes in an activity result AFTER
    // that. Reload the store + re-verify the active driver on every
    // ON_RESUME — returning from the file picker refreshes the sheet.
    val lifecycleOwner = LocalLifecycleOwner.current
    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME) {
                slots = DriverSlotStore.load(context)
                val mode = Px5Settings.driverMode.value
                activeMode = mode
                scope.launch {
                    if (mode > 0) verifyAndRefresh(mode) else refreshSummary()
                }
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
            .background(px5Colors().sheet)
            .border(1.dp, px5Colors().hairline, RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
            .padding(24.dp)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            // Header
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    imageVector = Icons.Default.Memory,
                    contentDescription = null,
                    tint = px5Colors().accentGlow,
                    modifier = Modifier.size(26.dp)
                )
                Spacer(modifier = Modifier.width(10.dp))
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = "GPU Driver Manager",
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        color = px5Colors().text,
                        fontFamily = TitilliumFontFamily
                    )
                    Text(
                        text = "System Adreno ICD + imported Turnip drivers (libadrenotools)",
                        fontSize = 12.sp,
                        color = px5Colors().textSecondary,
                        fontFamily = TitilliumFontFamily
                    )
                }
                IconButton(
                    onClick = onDismiss,
                    modifier = Modifier
                        .size(36.dp)
                        .clip(CircleShape)
                        .background(px5Colors().control)
                ) {
                    Icon(
                        imageVector = Icons.Default.Close,
                        contentDescription = "Close",
                        tint = px5Colors().text,
                        modifier = Modifier.size(18.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            LazyColumn(
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(max = 340.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                // System driver — always present, mode 0.
                item {
                    DriverRow(
                        title = "System Qualcomm Adreno driver",
                        subtitle = "Default device ICD (libvulkan.so)",
                        selected = activeMode == 0,
                        enabled = !busy,
                        onSelect = {
                            busy = true
                            scope.launch {
                                fexCoreWrapper?.nativeSetDriverMode(0)
                                Px5Settings.setDriverMode(0)
                                activeMode = 0
                                soundManager.playActivationSound()
                                verifyAndRefresh(0)
                                busy = false
                            }
                        }
                    )
                }
                // Imported slots — mode i+1, only if the .so still exists.
                items(slots) { slot ->
                    val exists = remember(slot.soPath) { File(slot.soPath).isFile }
                    val idx = slots.indexOf(slot) + 1
                    DriverRow(
                        title = slot.label,
                        subtitle = if (exists) slot.soPath
                        else "${slot.soPath}  —  file missing, re-import required",
                        selected = activeMode == idx && exists,
                        enabled = !busy && exists,
                        showDelete = exists,
                        onSelect = {
                            busy = true
                            scope.launch {
                                fexCoreWrapper?.nativeSetDriverMode(idx)
                                Px5Settings.setDriverMode(idx)
                                activeMode = idx
                                soundManager.playActivationSound()
                                verifyAndRefresh(idx)
                                busy = false
                            }
                        },
                        onDelete = {
                            if (exists && !busy) {
                                busy = true
                                scope.launch {
                                    DriverSlotStore.remove(context, idx - 1)
                                    val remaining = DriverSlotStore.load(context)
                                    // Rebuild the native registry so slot ids
                                    // match the persisted order again.
                                    fexCoreWrapper?.nativeClearDriverSlots()
                                    remaining.forEachIndexed { i, s ->
                                        fexCoreWrapper?.nativeRegisterDriverSlot(
                                            s.label, s.soPath, s.soname)
                                    }
                                    val newMode =
                                        if (activeMode == idx) 0 else activeMode.coerceAtMost(remaining.size)
                                    fexCoreWrapper?.nativeSetDriverMode(newMode)
                                    Px5Settings.setDriverMode(newMode)
                                    slots = remaining
                                    activeMode = newMode
                                    soundManager.playActivationSound()
                                    verifyAndRefresh(newMode)
                                    busy = false
                                }
                            }
                        }
                    )
                }
            }

            if (slots.isEmpty()) {
                Text(
                    text = "No imported drivers yet. Import a Turnip/Mesa driver package " +
                            "(vulkan.turnip.so, libvulkan.so, libvulkan_adreno.so — arm64-v8a).",
                    fontSize = 12.sp,
                    color = px5Colors().textSecondary,
                    fontFamily = TitilliumFontFamily,
                    modifier = Modifier.padding(vertical = 8.dp)
                )
            }

            Spacer(modifier = Modifier.height(14.dp))

            Button(
                enabled = !busy,
                onClick = {
                    soundManager.playActivationSound()
                    onImportCustomDriverClick()
                },
                colors = ButtonDefaults.buttonColors(
                    containerColor = PS5AccentBlue, contentColor = Color.White
                ),
                shape = RoundedCornerShape(14.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Icon(Icons.Default.Add, contentDescription = null, modifier = Modifier.size(18.dp))
                Spacer(Modifier.width(8.dp))
                Text("Import driver package (.zip)", fontWeight = FontWeight.Bold, fontFamily = TitilliumFontFamily)
            }

            Spacer(modifier = Modifier.height(12.dp))

            // Live engine summary — includes driverVerified= state.
            MonoSummary(summary)
        }
    }
}

@Composable
private fun DriverRow(
    title: String,
    subtitle: String,
    selected: Boolean,
    enabled: Boolean,
    showDelete: Boolean = false,
    onSelect: () -> Unit,
    onDelete: () -> Unit = {}
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(
                if (selected) PS5AccentBlue.copy(alpha = 0.28f)
                else px5Colors().card
            )
            .border(
                width = if (selected) 1.5.dp else 1.dp,
                color = if (selected) px5Colors().accentGlow else px5Colors().controlStrong,
                shape = RoundedCornerShape(12.dp)
            )
            .clickable(enabled = enabled, onClick = onSelect)
            .padding(horizontal = 14.dp, vertical = 12.dp)
    ) {
        if (selected) {
            Icon(
                imageVector = Icons.Default.Check,
                contentDescription = "Active",
                tint = px5Colors().accentGlow,
                modifier = Modifier.size(20.dp)
            )
            Spacer(Modifier.width(10.dp))
        }
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = title,
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                color = px5Colors().text,
                fontFamily = TitilliumFontFamily
            )
            Text(
                text = subtitle,
                fontSize = 11.sp,
                color = px5Colors().textSecondary,
                fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace,
                maxLines = 2
            )
        }
        if (showDelete) {
            IconButton(onClick = onDelete, modifier = Modifier.size(34.dp)) {
                Icon(
                    imageVector = Icons.Default.Delete,
                    contentDescription = "Remove driver",
                    tint = px5Colors().textSecondary,
                    modifier = Modifier.size(18.dp)
                )
            }
        }
    }
}

@Composable
private fun MonoSummary(text: String) {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(Color.Black.copy(alpha = 0.55f))
            .border(1.dp, PS5AccentBlue.copy(alpha = 0.4f), RoundedCornerShape(10.dp))
            .padding(10.dp)
    ) {
        Text(
            text = text,
            fontSize = 11.sp,
            color = px5Colors().infoMono,
            fontFamily = androidx.compose.ui.text.font.FontFamily.Monospace
        )
    }
}
