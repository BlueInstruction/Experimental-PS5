package com.px5.emulator.ui

import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.material3.VerticalDivider
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.GameEntity
import com.px5.emulator.GameViewModel
import com.px5.emulator.R
import com.px5.emulator.SoundManager
import com.px5.emulator.core.FexCoreWrapper
import java.text.DateFormat
import java.util.Date

/**
 * PS5HomeScreen — the real game library.
 *
 * Honesty rules enforced here:
 *  * No seeded demo games: an empty library shows an empty state with
 *    real import actions. Nothing invents content.
 *  * Details show only real fields (format, title id, byte size from
 *    disk, version parsed from param.json / PKG SFO, real last-played
 *    and accumulated play time tracked by the app itself).
 *  * No fake trophy bars, no "82% completed" activity cards, no store.
 *
 * Orientation: the shell adapts — landscape keeps the PS5 carousel plus
 * side detail panel, portrait switches to a vertical cover grid with the
 * detail panel below. Both directions are fully usable (the old build
 * was landscape-locked).
 */
@Composable
fun PS5HomeScreen(
    games: List<GameEntity>,
    gameViewModel: GameViewModel,
    soundManager: SoundManager,
    fexCoreStatus: String,
    fexCoreWrapper: FexCoreWrapper? = null,
    onGameSelected: (String) -> Unit,
    onOpenSettings: () -> Unit,
    onOpenSearch: () -> Unit,
    onImportFileClick: () -> Unit,
    onImportFolderClick: () -> Unit
) {
    var selectedTab by remember { mutableStateOf(0) } // 0: Games
    var selectedIndex by remember { mutableStateOf(0) }
    var showControlCenter by remember { mutableStateOf(false) }

    val displayedList = remember(games, selectedTab) {
        if (selectedTab == 0) games else emptyList()
    }
    val selectedGame = displayedList.getOrNull(selectedIndex) ?: displayedList.firstOrNull()

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(PS5DarkBackground)
    ) {
        // Ambient backdrop
        Crossfade(
            targetState = selectedGame?.id,
            animationSpec = tween(durationMillis = 400),
            label = "BackdropCrossfade"
        ) { _ ->
            val backdropPainter = safePainterResource(id = R.drawable.ps5background_all)
            if (backdropPainter != null) {
                Image(
                    painter = backdropPainter,
                    contentDescription = null,
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize(),
                    alpha = 0.35f
                )
            }
        }

        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            Color.Black.copy(alpha = 0.5f),
                            Color(0xFF0B0E14).copy(alpha = 0.85f),
                            Color(0xFF0B0E14)
                        )
                    )
                )
        )

        Column(modifier = Modifier.fillMaxSize()) {
            PS5TopHeader(
                selectedTab = selectedTab,
                onTabSelected = { tab ->
                    selectedTab = tab
                    selectedIndex = 0
                    soundManager.playNavigationSound()
                },
                onSearchClick = {
                    soundManager.playNavigationSound()
                    onOpenSearch()
                },
                onSettingsClick = {
                    soundManager.playNavigationSound()
                    onOpenSettings()
                },
                onProfileClick = {
                    soundManager.playNavigationSound()
                    showControlCenter = true
                }
            )

            Spacer(modifier = Modifier.height(8.dp))

            // Engine telemetry strip (real JNI polling)
            EngineStatusStrip(
                fexCoreWrapper = fexCoreWrapper,
                cpuStatus = fexCoreStatus
            )

            Spacer(modifier = Modifier.height(12.dp))

            BoxWithConstraints(modifier = Modifier.fillMaxSize()) {
                val isLandscape = maxWidth > 700.dp
                if (isLandscape) {
                    LandscapeLibrary(
                        games = displayedList,
                        selectedIndex = selectedIndex,
                        selectedGame = selectedGame,
                        onSelect = { index ->
                            selectedIndex = index
                            soundManager.playNavigationSound()
                        },
                        onActivate = { game ->
                            soundManager.playActivationSound()
                            onGameSelected(game.path)
                        },
                        onImportFileClick = onImportFileClick,
                        onImportFolderClick = onImportFolderClick,
                        gameViewModel = gameViewModel
                    )
                } else {
                    PortraitLibrary(
                        games = displayedList,
                        onGameSelected = { game ->
                            soundManager.playActivationSound()
                            onGameSelected(game.path)
                        },
                        onImportFileClick = onImportFileClick,
                        onImportFolderClick = onImportFolderClick,
                        gameViewModel = gameViewModel
                    )
                }
            }
        }

        // Overlay: Control Center (real engine telemetry + audio toggles)
        if (showControlCenter) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.6f))
                    .clickable { showControlCenter = false },
                contentAlignment = Alignment.BottomCenter
            ) {
                PS5ControlCenterSheet(
                    soundManager = soundManager,
                    fexCoreStatus = fexCoreStatus,
                    fexCoreWrapper = fexCoreWrapper,
                    onDismiss = { showControlCenter = false },
                    onRestartRequested = {
                        showControlCenter = false
                    }
                )
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Landscape: carousel + selected-game detail panel
// ---------------------------------------------------------------------------

@Composable
private fun LandscapeLibrary(
    games: List<GameEntity>,
    selectedIndex: Int,
    selectedGame: GameEntity?,
    onSelect: (Int) -> Unit,
    onActivate: (GameEntity) -> Unit,
    onImportFileClick: () -> Unit,
    onImportFolderClick: () -> Unit,
    gameViewModel: GameViewModel
) {
    Column(modifier = Modifier.fillMaxSize()) {
        LazyRow(
            contentPadding = PaddingValues(horizontal = 40.dp),
            horizontalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            itemsIndexed(games) { index, item ->
                PX5GameCardTile(
                    game = item,
                    isSelected = index == selectedIndex,
                    onClick = {
                        if (index == selectedIndex) onActivate(item) else onSelect(index)
                    }
                )
            }
            item {
                PX5GameCardTile(
                    game = games.firstOrNull() ?: GameEntity(id = "add", name = "", path = ""),
                    isSelected = false,
                    isAddTile = true,
                    onClick = onImportFileClick
                )
            }
        }

        Spacer(modifier = Modifier.height(20.dp))

        Box(
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
                .padding(horizontal = 40.dp)
        ) {
            if (games.isEmpty()) {
                EmptyLibraryState(
                    onImportFileClick = onImportFileClick,
                    onImportFolderClick = onImportFolderClick
                )
            } else if (selectedGame != null) {
                GameDetailPanel(
                    game = selectedGame,
                    gameViewModel = gameViewModel,
                    onPlay = { onActivate(selectedGame) },
                    modifier = Modifier.fillMaxSize()
                )
            }
        }

        Spacer(modifier = Modifier.height(12.dp))
    }
}

// ---------------------------------------------------------------------------
// Portrait: vertical cover grid + import CTA
// ---------------------------------------------------------------------------

@Composable
private fun PortraitLibrary(
    games: List<GameEntity>,
    onGameSelected: (GameEntity) -> Unit,
    onImportFileClick: () -> Unit,
    onImportFolderClick: () -> Unit,
    gameViewModel: GameViewModel
) {
    if (games.isEmpty()) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Spacer(modifier = Modifier.weight(1f))
            EmptyLibraryState(
                onImportFileClick = onImportFileClick,
                onImportFolderClick = onImportFolderClick
            )
            Spacer(modifier = Modifier.weight(1.4f))
        }
    } else {
        LazyVerticalGrid(
            columns = GridCells.Adaptive(minSize = 110.dp),
            contentPadding = PaddingValues(horizontal = 20.dp, vertical = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(14.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp),
            modifier = Modifier.fillMaxSize()
        ) {
            items(games, key = { it.id }) { game ->
                PX5GameGridTile(game = game, onClick = { onGameSelected(game) })
            }
            item {
                PX5GameGridTile(game = games.first(), isAddTile = true, onClick = onImportFileClick)
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Empty state — the honest front door
// ---------------------------------------------------------------------------

@Composable
private fun EmptyLibraryState(
    onImportFileClick: () -> Unit,
    onImportFolderClick: () -> Unit
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 24.dp)
    ) {
        Icon(
            imageVector = Icons.Default.FolderOpen,
            contentDescription = null,
            tint = PS5TextSecondary,
            modifier = Modifier.size(56.dp)
        )
        Spacer(modifier = Modifier.height(14.dp))
        Text(
            text = "Your library is empty",
            fontSize = 22.sp,
            fontWeight = FontWeight.Bold,
            color = PS5TextPrimary,
            fontFamily = TitilliumFontFamily
        )
        Spacer(modifier = Modifier.height(8.dp))
        Text(
            text = "Add a PS5 game: a decrypted dump folder (eboot.bin + param.json), " +
                    "a .pkg, or an .elf file. Covers are taken from the game's sce_sys icons.",
            fontSize = 13.sp,
            color = PS5TextSecondary,
            fontFamily = TitilliumFontFamily,
            textAlign = androidx.compose.ui.text.style.TextAlign.Center,
            modifier = Modifier.padding(horizontal = 24.dp)
        )
        Spacer(modifier = Modifier.height(20.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Button(
                onClick = onImportFileClick,
                colors = ButtonDefaults.buttonColors(
                    containerColor = PS5AccentBlue,
                    contentColor = Color.White
                ),
                shape = RoundedCornerShape(20.dp)
            ) {
                Text("Add file (.pkg / .elf)", fontWeight = FontWeight.Bold, fontSize = 13.sp)
            }
            Button(
                onClick = onImportFolderClick,
                colors = ButtonDefaults.buttonColors(
                    containerColor = Color.White.copy(alpha = 0.12f),
                    contentColor = Color.White
                ),
                shape = RoundedCornerShape(20.dp)
            ) {
                Text("Add folder (dump)", fontWeight = FontWeight.Bold, fontSize = 13.sp)
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Detail panel — real fields only
// ---------------------------------------------------------------------------

@Composable
private fun GameDetailPanel(
    game: GameEntity,
    gameViewModel: GameViewModel,
    onPlay: () -> Unit,
    modifier: Modifier = Modifier
) {
    var showMenu by remember { mutableStateOf(false) }

    Column(
        modifier = modifier.verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            // Cover thumbnail
            val cover = rememberGameCover(game.coverPath)
            if (cover != null) {
                Image(
                    bitmap = cover,
                    contentDescription = null,
                    contentScale = ContentScale.Crop,
                    modifier = Modifier
                        .width(72.dp)
                        .height(102.dp)
                        .clip(RoundedCornerShape(10.dp))
                        .border(1.dp, Color.White.copy(alpha = 0.2f), RoundedCornerShape(10.dp))
                )
                Spacer(modifier = Modifier.width(16.dp))
            }

            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = game.name,
                    fontSize = 30.sp,
                    fontWeight = FontWeight.Bold,
                    color = PS5TextPrimary,
                    fontFamily = TitilliumFontFamily,
                    maxLines = 2,
                    overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis
                )
                Spacer(modifier = Modifier.height(8.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    FormatBadge(game.format)
                    if (game.titleId.isNotBlank()) {
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = game.titleId,
                            color = PS5TextSecondary,
                            fontSize = 13.sp,
                            fontFamily = TitilliumFontFamily,
                            fontWeight = FontWeight.SemiBold
                        )
                    }
                }
            }

            // Options menu (favorite / remove)
            Box {
                IconButton(
                    onClick = { showMenu = true },
                    modifier = Modifier
                        .size(44.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.1f))
                ) {
                    Icon(
                        imageVector = Icons.Default.MoreVert,
                        contentDescription = "More",
                        tint = PS5TextPrimary
                    )
                }
                DropdownMenu(expanded = showMenu, onDismissRequest = { showMenu = false }) {
                    DropdownMenuItem(
                        text = { Text(if (game.isFavorite) "Remove from Favorites" else "Add to Favorites") },
                        onClick = {
                            gameViewModel.toggleFavorite(game.id, !game.isFavorite)
                            showMenu = false
                        }
                    )
                    DropdownMenuItem(
                        text = { Text("Remove from Library") },
                        onClick = {
                            gameViewModel.delete(game.id)
                            showMenu = false
                        }
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(6.dp))

        // Real metadata lines — every value comes from the import record.
        DetailLine("Status", game.status)
        if (game.version.isNotBlank()) DetailLine("Version", game.version)
        DetailLine("Size", formatBytes(game.sizeBytes))
        if (game.lastPlayedMillis > 0) {
            DetailLine(
                "Last played",
                DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT)
                    .format(Date(game.lastPlayedMillis))
            )
        } else {
            DetailLine("Last played", "Never")
        }
        DetailLine("Play time", formatDuration(game.playTimeSeconds))
        if (game.format == "DUMP" || game.format == "ELF") {
            DetailLine("Location", game.path, mono = true)
        }

        Spacer(modifier = Modifier.height(16.dp))

        Button(
            onClick = onPlay,
            colors = ButtonDefaults.buttonColors(
                containerColor = Color.White,
                contentColor = Color.Black
            ),
            shape = RoundedCornerShape(24.dp),
            contentPadding = PaddingValues(horizontal = 32.dp, vertical = 14.dp)
        ) {
            Icon(imageVector = Icons.Default.PlayArrow, contentDescription = null, modifier = Modifier.size(20.dp))
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = "Open in Engine",
                fontWeight = FontWeight.Bold,
                fontSize = 15.sp,
                fontFamily = TitilliumFontFamily
            )
        }

        Spacer(modifier = Modifier.height(8.dp))
    }
}

@Composable
private fun FormatBadge(format: String) {
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(6.dp))
            .background(PS5AccentBlue.copy(alpha = 0.25f))
            .border(1.dp, PS5AccentGlow.copy(alpha = 0.5f), RoundedCornerShape(6.dp))
            .padding(horizontal = 8.dp, vertical = 3.dp)
    ) {
        Text(
            text = format,
            fontSize = 11.sp,
            fontWeight = FontWeight.Bold,
            color = PS5AccentGlow,
            fontFamily = TitilliumFontFamily
        )
    }
}

@Composable
private fun DetailLine(label: String, value: String, mono: Boolean = false) {
    Row(modifier = Modifier.fillMaxWidth()) {
        Text(
            text = label,
            fontSize = 13.sp,
            color = PS5TextSecondary,
            fontFamily = TitilliumFontFamily,
            modifier = Modifier.width(110.dp)
        )
        Text(
            text = value,
            fontSize = 13.sp,
            color = PS5TextPrimary,
            fontFamily = if (mono) androidx.compose.ui.text.font.FontFamily.Monospace else TitilliumFontFamily,
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
