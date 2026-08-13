package com.px5.emulator.ui

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import androidx.compose.animation.Crossfade
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.GameEntity
import com.px5.emulator.GameViewModel
import com.px5.emulator.R
import com.px5.emulator.SoundManager

@Composable
fun PS5HomeScreen(
    games: List<GameEntity>,
    gameViewModel: GameViewModel,
    soundManager: SoundManager,
    fexCoreStatus: String,
    onGameSelected: (String) -> Unit,
    onOpenStore: () -> Unit,
    onOpenSettings: () -> Unit,
    onOpenSearch: () -> Unit,
    onAddGameClick: () -> Unit
) {
    val context = LocalContext.current
    var selectedTab by remember { mutableStateOf(0) } // 0: Games, 1: Media
    var selectedIndex by remember { mutableStateOf(0) }
    
    // Battery Receiver
    var batteryLevel by remember { mutableStateOf(-1) }
    var batteryIsCharging by remember { mutableStateOf(false) }

    // Overlay Drawer States
    var showControlCenter by remember { mutableStateOf(false) }
    var showNotifications by remember { mutableStateOf(false) }

    DisposableEffect(context) {
        val batteryReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                val level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
                val scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
                if (level != -1 && scale != -1) {
                    batteryLevel = (level * 100 / scale.toFloat()).toInt()
                }
                val status = intent.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
                batteryIsCharging = status == BatteryManager.BATTERY_STATUS_CHARGING ||
                        status == BatteryManager.BATTERY_STATUS_FULL
            }
        }
        context.registerReceiver(batteryReceiver, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        onDispose {
            context.unregisterReceiver(batteryReceiver)
        }
    }

    // Filter games by active top tab (Games vs Media)
    val displayedList = remember(games, selectedTab) {
        val storeTile = GameEntity(
            id = "ps5_store",
            name = "PlayStation Store",
            path = "store",
            category = "Store"
        )
        val addTile = GameEntity(
            id = "ps5_add_game",
            name = "Install Game",
            path = "add",
            category = "Install"
        )

        if (selectedTab == 0) {
            val gameItems = games.filter { it.category != "Media" }
            listOf(storeTile) + gameItems + listOf(addTile)
        } else {
            val mediaItems = games.filter { it.category == "Media" }
            mediaItems
        }
    }

    val selectedGame = displayedList.getOrNull(selectedIndex) ?: displayedList.firstOrNull()

    // Sample Notifications
    val sampleNotifications = remember {
        listOf(
            PS5NotificationItem(
                id = "notif_1",
                title = "Trophy Unlocked!",
                description = "Platinum Trophy in Astro's Playroom",
                timeAgo = "10m ago",
                iconRes = R.drawable.ps5_trophy_gold
            ),
            PS5NotificationItem(
                id = "notif_2",
                title = "DualSense Controller Connected",
                description = "Bluetooth Controller 1 Active (Haptics ON)",
                timeAgo = "1h ago",
                iconRes = R.drawable.ic_dualsense_ps
            ),
            PS5NotificationItem(
                id = "notif_3",
                title = "FEXCore Execution Core",
                description = "ARM64 Bionic JNI Translation Engine Ready",
                timeAgo = "2h ago",
                iconRes = R.drawable.ps5_intro_logo
            )
        )
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(PS5DarkBackground)
    ) {
        // Dynamic Fullscreen Hero Backdrop
        Crossfade(
            targetState = selectedGame?.id,
            animationSpec = tween(durationMillis = 400),
            label = "BackdropCrossfade"
        ) { targetId ->
            val bgRes = if (targetId == "ps5_store") R.drawable.ps5_store_background else R.drawable.ps5_background_all
            val backdropPainter = safePainterResource(id = bgRes)
            if (backdropPainter != null) {
                Image(
                    painter = backdropPainter,
                    contentDescription = "Backdrop",
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize(),
                    alpha = 0.5f
                )
            }
        }

        // Atmosphere Dark Gradient Overlay
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
            // PS5 Top Navigation Header
            PS5TopHeader(
                selectedTab = selectedTab,
                onTabSelected = { tab ->
                    selectedTab = tab
                    selectedIndex = 0
                    soundManager.playNavigationSound()
                },
                notificationCount = sampleNotifications.size,
                batteryLevel = batteryLevel,
                batteryIsCharging = batteryIsCharging,
                onSearchClick = {
                    soundManager.playNavigationSound()
                    onOpenSearch()
                },
                onSettingsClick = {
                    soundManager.playNavigationSound()
                    onOpenSettings()
                },
                onNotificationsClick = {
                    soundManager.playNavigationSound()
                    showNotifications = !showNotifications
                },
                onProfileClick = {
                    soundManager.playNavigationSound()
                    showControlCenter = true
                }
            )

            Spacer(modifier = Modifier.height(16.dp))

            // Game Tile Carousel (Top Row)
            LazyRow(
                contentPadding = PaddingValues(horizontal = 40.dp),
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                itemsIndexed(displayedList) { index, item ->
                    PX5GameCardTile(
                        game = item,
                        isSelected = index == selectedIndex,
                        onClick = {
                            if (index == selectedIndex) {
                                soundManager.playActivationSound()
                                when (item.id) {
                                    "ps5_store" -> onOpenStore()
                                    "ps5_add_game" -> onAddGameClick()
                                    else -> onGameSelected(item.path)
                                }
                            } else {
                                selectedIndex = index
                                soundManager.playNavigationSound()
                            }
                        }
                    )
                }
            }

            Spacer(modifier = Modifier.height(36.dp))

            // Selected Game Details Panel
            Box(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .padding(horizontal = 40.dp)
            ) {
                if (selectedGame != null) {
                    Column(modifier = Modifier.fillMaxSize()) {
                        // Title
                        Text(
                            text = selectedGame.name,
                            fontSize = 38.sp,
                            fontWeight = FontWeight.Bold,
                            color = PS5TextPrimary,
                            fontFamily = TitilliumFontFamily
                        )

                        Spacer(modifier = Modifier.height(8.dp))

                        // Category & Details Subtitle
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            BadgePill(text = selectedGame.category)
                            Spacer(modifier = Modifier.width(10.dp))
                            Text(
                                text = "${selectedGame.developer} • Version ${selectedGame.version} • ${selectedGame.sizeGb}",
                                color = PS5TextSecondary,
                                fontSize = 14.sp,
                                fontFamily = TitilliumFontFamily
                            )
                        }

                        Spacer(modifier = Modifier.height(6.dp))

                        Text(
                            text = "Last Played: ${selectedGame.lastPlayed} • Play Time: ${selectedGame.playTime}",
                            color = PS5TextSecondary.copy(alpha = 0.8f),
                            fontSize = 13.sp,
                            fontFamily = TitilliumFontFamily
                        )

                        Spacer(modifier = Modifier.height(24.dp))

                        // Interactive Action Buttons Row
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(16.dp)
                        ) {
                            // Primary Play / Resume Button
                            Button(
                                onClick = {
                                    soundManager.playActivationSound()
                                    when (selectedGame.id) {
                                        "ps5_store" -> onOpenStore()
                                        "ps5_add_game" -> onAddGameClick()
                                        else -> onGameSelected(selectedGame.path)
                                    }
                                },
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = Color.White,
                                    contentColor = Color.Black
                                ),
                                shape = RoundedCornerShape(24.dp),
                                contentPadding = PaddingValues(horizontal = 32.dp, vertical = 14.dp)
                            ) {
                                Icon(imageVector = Icons.Default.PlayArrow, contentDescription = "Play", modifier = Modifier.size(20.dp))
                                Spacer(modifier = Modifier.width(8.dp))
                                Text(
                                    text = if (selectedGame.id == "ps5_store") "Explore Store" else if (selectedGame.id == "ps5_add_game") "Add Game" else "Play Game",
                                    fontWeight = FontWeight.Bold,
                                    fontSize = 16.sp,
                                    fontFamily = TitilliumFontFamily
                                )
                            }

                            // Options Button
                            if (selectedGame.id != "ps5_store" && selectedGame.id != "ps5_add_game") {
                                var showMenu by remember { mutableStateOf(false) }

                                Box {
                                    IconButton(
                                        onClick = { showMenu = true },
                                        modifier = Modifier
                                            .size(48.dp)
                                            .clip(CircleShape)
                                            .background(Color.White.copy(alpha = 0.1f))
                                    ) {
                                        Icon(imageVector = Icons.Default.MoreVert, contentDescription = "More", tint = PS5TextPrimary)
                                    }

                                    DropdownMenu(
                                        expanded = showMenu,
                                        onDismissRequest = { showMenu = false }
                                    ) {
                                        DropdownMenuItem(
                                            text = { Text(if (selectedGame.isFavorite) "Remove from Favorites" else "Add to Favorites") },
                                            onClick = {
                                                gameViewModel.toggleFavorite(selectedGame.id, !selectedGame.isFavorite)
                                                showMenu = false
                                            }
                                        )
                                        DropdownMenuItem(
                                            text = { Text("Delete Game") },
                                            onClick = {
                                                gameViewModel.delete(selectedGame.id)
                                                showMenu = false
                                            }
                                        )
                                    }
                                }
                            }
                        }

                        Spacer(modifier = Modifier.height(28.dp))

                        // Trophy Tracker & Activity Cards Section
                        if (selectedGame.trophiesTotal > 0) {
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(32.dp)
                            ) {
                                // Trophy Summary Bar
                                PS5TrophySummaryBar(
                                    unlocked = selectedGame.trophiesUnlocked,
                                    total = selectedGame.trophiesTotal,
                                    bronze = selectedGame.bronzeCount,
                                    silver = selectedGame.silverCount,
                                    gold = selectedGame.goldCount,
                                    modifier = Modifier.weight(1.2f)
                                )

                                // Activity Card
                                PS5ActivityCard(
                                    title = "Resume Activity",
                                    subtitle = "Current Chapter • 82% Completed",
                                    progressText = "Quick Resume Ready",
                                    onClick = {
                                        soundManager.playActivationSound()
                                        onGameSelected(selectedGame.path)
                                    },
                                    modifier = Modifier.weight(0.8f)
                                )
                            }
                        }
                    }
                }
            }

            // Bottom Prompt Bar
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 40.dp, vertical = 20.dp)
                    .windowInsetsPadding(WindowInsets.navigationBars),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Control Center Prompt
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(12.dp))
                        .background(Color.White.copy(alpha = 0.08f))
                        .clickable {
                            soundManager.playNavigationSound()
                            showControlCenter = true
                        }
                        .padding(horizontal = 14.dp, vertical = 6.dp)
                ) {
                    Text(
                        text = "Press PS for Control Center",
                        fontSize = 12.sp,
                        color = PS5TextPrimary,
                        fontFamily = TitilliumFontFamily,
                        fontWeight = FontWeight.SemiBold
                    )
                }

                Spacer(modifier = Modifier.weight(1f))

                // DualSense Button Prompts
                DualSenseButtonPrompts()
            }
        }

        // Overlay: Control Center Quick Menu
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
                    onDismiss = { showControlCenter = false },
                    onRestartRequested = {
                        showControlCenter = false
                    }
                )
            }
        }

        // Overlay: Notifications Drawer
        if (showNotifications) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.5f))
                    .clickable { showNotifications = false },
                contentAlignment = Alignment.CenterEnd
            ) {
                PS5NotificationsDrawer(
                    notifications = sampleNotifications,
                    onDismiss = { showNotifications = false }
                )
            }
        }
    }
}

@Composable
private fun BadgePill(text: String) {
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(6.dp))
            .background(PS5AccentBlue.copy(alpha = 0.25f))
            .border(1.dp, PS5AccentGlow.copy(alpha = 0.5f), RoundedCornerShape(6.dp))
            .padding(horizontal = 8.dp, vertical = 3.dp)
    ) {
        Text(
            text = text,
            fontSize = 11.sp,
            fontWeight = FontWeight.Bold,
            color = PS5AccentGlow,
            fontFamily = TitilliumFontFamily
        )
    }
}

