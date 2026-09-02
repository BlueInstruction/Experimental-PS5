package com.px5.emulator.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.InsertDriveFile
import androidx.compose.material.icons.filled.ScreenRotation
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.material3.TabRowDefaults
import androidx.compose.material3.TabRowDefaults.tabIndicatorOffset
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.GameEntity
import com.px5.emulator.GameViewModel
import com.px5.emulator.SoundManager
import com.px5.emulator.core.FexCoreWrapper
import com.px5.emulator.core.Px5Settings
import java.text.DateFormat
import java.util.Date

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PS5HomeScreen(
    games: List<GameEntity>,
    gameViewModel: GameViewModel,
    soundManager: SoundManager,
    fexCoreStatus: String,
    fexCoreWrapper: FexCoreWrapper? = null,
    onGameSelected: (String) -> Unit,
    onOpenSettings: () -> Unit,
    onImportFileClick: () -> Unit,
    onImportFolderClick: () -> Unit
) {
    var showAddMenu by remember { mutableStateOf(false) }
    var showGameDetails by remember { mutableStateOf<GameEntity?>(null) }
    val context = androidx.compose.ui.platform.LocalContext.current

    val onRotate = {
        soundManager.playNavigationSound()
        val landscapeNow = context.resources.configuration.orientation ==
                android.content.res.Configuration.ORIENTATION_LANDSCAPE
        Px5Settings.setOrientationMode(if (landscapeNow) 2 else 1)
        (context as? android.app.Activity)?.let { Px5Settings.applyOrientation(it) }
        Unit
    }

    val darkBackground = px5Colors().background
    val surfaceColor = px5Colors().surface
    val textColor = px5Colors().text
    val textSecondary = px5Colors().textSecondary
    val accentColor = px5Colors().accent

    Scaffold(
        containerColor = darkBackground,
        topBar = {
            TopAppBar(
                title = { 
                    Text("PSX5 Emulator", fontWeight = FontWeight.Bold, color = textColor) 
                },
                actions = {
                    IconButton(onClick = onRotate) {
                        Icon(Icons.Default.ScreenRotation, contentDescription = "Rotate", tint = textColor)
                    }
                    Box {
                        IconButton(onClick = { showAddMenu = true }) {
                            Icon(Icons.Default.Add, contentDescription = "Install", tint = textColor)
                        }
                        DropdownMenu(
                            expanded = showAddMenu,
                            onDismissRequest = { showAddMenu = false },
                            modifier = Modifier.background(surfaceColor)
                        ) {
                            DropdownMenuItem(
                                text = { Text("Install firmware / folder", color = textColor) },
                                onClick = { showAddMenu = false; onImportFolderClick() },
                                leadingIcon = { Icon(Icons.Default.FolderOpen, contentDescription = null, tint = textColor) }
                            )
                            DropdownMenuItem(
                                text = { Text("Install .pkg / .elf", color = textColor) },
                                onClick = { showAddMenu = false; onImportFileClick() },
                                leadingIcon = { Icon(Icons.Default.InsertDriveFile, contentDescription = null, tint = textColor) }
                            )
                        }
                    }
                    IconButton(onClick = { soundManager.playNavigationSound(); onOpenSettings() }) {
                        Icon(Icons.Default.Settings, contentDescription = "Settings", tint = textColor)
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = surfaceColor
                )
            )
        }
    ) { innerPadding ->
        Column(modifier = Modifier.padding(innerPadding).fillMaxSize()) {
            if (games.isEmpty()) {
                Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Icon(
                            imageVector = Icons.Default.FolderOpen,
                            contentDescription = null,
                            tint = textSecondary,
                            modifier = Modifier.size(64.dp)
                        )
                        Spacer(modifier = Modifier.height(16.dp))
                        Text(
                            text = "No games installed",
                            color = textSecondary,
                            fontSize = 16.sp
                        )
                        Spacer(modifier = Modifier.height(16.dp))
                        Button(
                            onClick = onImportFolderClick,
                            colors = ButtonDefaults.buttonColors(containerColor = surfaceColor, contentColor = accentColor),
                            border = androidx.compose.foundation.BorderStroke(1.dp, accentColor)
                        ) {
                            Text("Install Game")
                        }
                    }
                }
            } else {
                LazyVerticalGrid(
                    columns = GridCells.Adaptive(minSize = 100.dp),
                    contentPadding = PaddingValues(12.dp),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                    modifier = Modifier.fillMaxSize()
                ) {
                    items(games, key = { it.id }) { game ->
                        EmulatorGameCard(
                            game = game,
                            onClick = {
                                soundManager.playActivationSound()
                                onGameSelected(game.path)
                            },
                            onLongClick = { showGameDetails = game },
                            surfaceColor = surfaceColor,
                            textColor = textColor,
                            textSecondary = textSecondary
                        )
                    }
                }
            }
        }
    }

    if (showGameDetails != null) {
        GameDetailDialog(
            game = showGameDetails!!,
            gameViewModel = gameViewModel,
            onDismiss = { showGameDetails = null },
            onPlay = {
                soundManager.playActivationSound()
                onGameSelected(showGameDetails!!.path)
                showGameDetails = null
            },
            surfaceColor = surfaceColor,
            textColor = textColor,
            textSecondary = textSecondary,
            accentColor = accentColor
        )
    }
}

@OptIn(androidx.compose.foundation.ExperimentalFoundationApi::class)
@Composable
private fun EmulatorGameCard(
    game: GameEntity,
    onClick: () -> Unit,
    onLongClick: () -> Unit,
    surfaceColor: androidx.compose.ui.graphics.Color,
    textColor: androidx.compose.ui.graphics.Color,
    textSecondary: androidx.compose.ui.graphics.Color
) {
    Card(
        shape = RoundedCornerShape(4.dp),
        colors = CardDefaults.cardColors(containerColor = surfaceColor),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp),
        modifier = Modifier
            .fillMaxWidth()
            .combinedClickable(
                onClick = onClick,
                onLongClick = onLongClick
            )
    ) {
        Column {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(0.75f)
                    .background(androidx.compose.ui.graphics.Color.Black),
                contentAlignment = Alignment.Center
            ) {
                val cover = rememberGameCover(game.coverPath)
                if (cover != null) {
                    Image(
                        bitmap = cover,
                        contentDescription = game.name,
                        contentScale = ContentScale.Crop,
                        modifier = Modifier.fillMaxSize()
                    )
                } else {
                    Text(
                        text = game.name.take(2).uppercase(),
                        color = textSecondary,
                        fontSize = 32.sp,
                        fontWeight = FontWeight.Bold
                    )
                }
            }
            Column(modifier = Modifier.padding(8.dp)) {
                Text(
                    text = game.name,
                    color = textColor,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = game.titleId.ifBlank { game.format },
                    color = textSecondary,
                    fontSize = 10.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }
    }
}

@Composable
private fun GameDetailDialog(
    game: GameEntity,
    gameViewModel: GameViewModel,
    onDismiss: () -> Unit,
    onPlay: () -> Unit,
    surfaceColor: androidx.compose.ui.graphics.Color,
    textColor: androidx.compose.ui.graphics.Color,
    textSecondary: androidx.compose.ui.graphics.Color,
    accentColor: androidx.compose.ui.graphics.Color
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = surfaceColor,
        titleContentColor = textColor,
        textContentColor = textSecondary,
        title = {
            Text(text = game.name, maxLines = 1, overflow = TextOverflow.Ellipsis, fontSize = 18.sp, fontWeight = FontWeight.Bold)
        },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                DetailLineDialog("Format", game.format, textColor, textSecondary)
                if (game.titleId.isNotBlank()) DetailLineDialog("Title ID", game.titleId, textColor, textSecondary)
                if (game.version.isNotBlank()) DetailLineDialog("Version", game.version, textColor, textSecondary)
                DetailLineDialog("Size", formatBytes(game.sizeBytes), textColor, textSecondary)
                if (game.lastPlayedMillis > 0) {
                    DetailLineDialog(
                        "Last Played",
                        DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT)
                            .format(Date(game.lastPlayedMillis)),
                        textColor, textSecondary
                    )
                } else {
                    DetailLineDialog("Last Played", "Never", textColor, textSecondary)
                }
                DetailLineDialog("Play Time", formatDuration(game.playTimeSeconds), textColor, textSecondary)
            }
        },
        confirmButton = {
            TextButton(onClick = onPlay) {
                Text("Start", color = accentColor, fontWeight = FontWeight.Bold)
            }
        },
        dismissButton = {
            TextButton(
                onClick = {
                    gameViewModel.delete(game.id)
                    onDismiss()
                }
            ) {
                Text("Uninstall", color = androidx.compose.ui.graphics.Color(0xFFEF5350))
            }
        }
    )
}

@Composable
private fun DetailLineDialog(label: String, value: String, textColor: androidx.compose.ui.graphics.Color, textSecondary: androidx.compose.ui.graphics.Color) {
    Row(modifier = Modifier.fillMaxWidth()) {
        Text(
            text = label,
            fontSize = 13.sp,
            color = textSecondary,
            modifier = Modifier.width(90.dp)
        )
        Text(
            text = value,
            fontSize = 13.sp,
            color = textColor,
            modifier = Modifier.weight(1f)
        )
    }
}

fun formatBytes(bytes: Long): String = when {
    bytes >= 1L shl 30 -> "%.1f GB".format(bytes / 1073741824.0)
    bytes >= 1L shl 20 -> "%.1f MB".format(bytes / 1048576.0)
    bytes >= 1L shl 10 -> "%.1f KB".format(bytes / 1024.0)
    bytes > 0 -> "$bytes B"
    else -> "—"
}

fun formatDuration(seconds: Long): String = when {
    seconds >= 3600 -> "%.1f h".format(seconds / 3600.0)
    seconds >= 60 -> "%d min".format(seconds / 60)
    seconds > 0 -> "${seconds}s"
    else -> "0 min"
}
