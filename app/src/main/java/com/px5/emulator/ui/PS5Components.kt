package com.px5.emulator.ui

import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsFocusedAsState
import androidx.compose.foundation.interaction.collectIsHoveredAsState
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.GameEntity
import com.px5.emulator.R

@Composable
fun PS5TopHeader(
    selectedTab: Int, // 0: Games, 1: Media
    onTabSelected: (Int) -> Unit,
    notificationCount: Int,
    batteryLevel: Int,
    batteryIsCharging: Boolean,
    onSearchClick: () -> Unit,
    onSettingsClick: () -> Unit,
    onNotificationsClick: () -> Unit,
    onProfileClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 40.dp, vertical = 20.dp)
            .windowInsetsPadding(WindowInsets.statusBars),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Navigation Tabs (Games / Media)
        Row(
            horizontalArrangement = Arrangement.spacedBy(28.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            HeaderTabItem(
                title = "Games",
                isSelected = selectedTab == 0,
                onClick = { onTabSelected(0) }
            )
            HeaderTabItem(
                title = "Media",
                isSelected = selectedTab == 1,
                onClick = { onTabSelected(1) }
            )
        }

        Spacer(modifier = Modifier.weight(1f))

        // System Action Icons
        Row(
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Search Button
            IconButton(
                onClick = onSearchClick,
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.08f))
            ) {
                Icon(
                    imageVector = Icons.Default.Search,
                    contentDescription = "Search",
                    tint = PS5TextPrimary,
                    modifier = Modifier.size(20.dp)
                )
            }

            // Notifications Button with badge
            Box {
                IconButton(
                    onClick = onNotificationsClick,
                    modifier = Modifier
                        .size(40.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.08f))
                ) {
                    Icon(
                        imageVector = Icons.Default.Notifications,
                        contentDescription = "Notifications",
                        tint = PS5TextPrimary,
                        modifier = Modifier.size(20.dp)
                    )
                }
                if (notificationCount > 0) {
                    Box(
                        modifier = Modifier
                            .align(Alignment.TopEnd)
                            .size(16.dp)
                            .clip(CircleShape)
                            .background(PS5AccentBlue),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = notificationCount.toString(),
                            color = Color.White,
                            fontSize = 10.sp,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }
            }

            // Settings Gear Button
            IconButton(
                onClick = onSettingsClick,
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .background(Color.White.copy(alpha = 0.08f))
            ) {
                Icon(
                    imageVector = Icons.Default.Settings,
                    contentDescription = "Settings",
                    tint = PS5TextPrimary,
                    modifier = Modifier.size(20.dp)
                )
            }

            // User Profile Avatar
            Box(
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .border(1.5.dp, PS5AccentBlue, CircleShape)
                    .clickable { onProfileClick() },
                contentAlignment = Alignment.Center
            ) {
                Image(
                    painter = painterResource(id = R.drawable.ps5_profile_picture),
                    contentDescription = "Profile Avatar",
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize()
                )
            }

            Spacer(modifier = Modifier.width(8.dp))

            // Controller & Battery Status
            Row(verticalAlignment = Alignment.CenterVertically) {
                Image(
                    painter = painterResource(id = R.drawable.ic_dualsense_ps),
                    contentDescription = "DualSense Controller",
                    modifier = Modifier.size(20.dp)
                )
                Spacer(modifier = Modifier.width(6.dp))
                if (batteryLevel >= 0) {
                    PS5BatteryIcon(level = batteryLevel, isCharging = batteryIsCharging)
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = "$batteryLevel%",
                        fontSize = 13.sp,
                        color = PS5TextSecondary,
                        fontFamily = TitilliumFontFamily,
                        fontWeight = FontWeight.SemiBold
                    )
                }
            }

            Spacer(modifier = Modifier.width(12.dp))

            // Real-time System Clock
            PS5SystemClock()
        }
    }
}

@Composable
private fun HeaderTabItem(
    title: String,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier.clickable { onClick() }
    ) {
        Text(
            text = title,
            fontSize = 20.sp,
            fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium,
            color = if (isSelected) PS5TextPrimary else PS5TextSecondary,
            fontFamily = TitilliumFontFamily
        )
        Spacer(modifier = Modifier.height(4.dp))
        Box(
            modifier = Modifier
                .width(if (isSelected) 28.dp else 0.dp)
                .height(3.dp)
                .clip(RoundedCornerShape(2.dp))
                .background(PS5AccentBlue)
        )
    }
}

@Composable
fun PX5GameCardTile(
    game: GameEntity,
    isSelected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val interactionSource = remember { MutableInteractionSource() }
    val isFocused by interactionSource.collectIsFocusedAsState()
    val isHovered by interactionSource.collectIsHoveredAsState()
    val isPressed by interactionSource.collectIsPressedAsState()
    val active = isSelected || isFocused || isHovered || isPressed

    val tileScale by animateFloatAsState(
        targetValue = if (active) 1.12f else 1.0f,
        animationSpec = tween(durationMillis = 200)
    )

    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = modifier
    ) {
        Box(
            modifier = Modifier
                .size(100.dp, 100.dp)
                .scale(tileScale)
                .clip(RoundedCornerShape(18.dp))
                .background(
                    if (active) Brush.linearGradient(
                        colors = listOf(Color(0xFF2E3D59), Color(0xFF1E2838))
                    ) else Brush.linearGradient(
                        colors = listOf(Color(0xFF1A2230), Color(0xFF121822))
                    )
                )
                .border(
                    width = if (active) 2.5.dp else 1.dp,
                    color = if (active) PS5TextPrimary else Color.White.copy(alpha = 0.15f),
                    shape = RoundedCornerShape(18.dp)
                )
                .clickable(
                    interactionSource = interactionSource,
                    indication = null,
                    onClick = onClick
                ),
            contentAlignment = Alignment.Center
        ) {
            if (game.id == "ps5_store") {
                Image(
                    painter = painterResource(id = R.drawable.ps5_playstation_store),
                    contentDescription = "PS Store",
                    modifier = Modifier.size(56.dp)
                )
            } else if (game.id == "ps5_add_game") {
                Icon(
                    imageVector = Icons.Default.Search,
                    contentDescription = "Add Game",
                    tint = PS5TextPrimary,
                    modifier = Modifier.size(40.dp)
                )
            } else {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center,
                    modifier = Modifier.padding(8.dp)
                ) {
                    Text(
                        text = game.name.take(2).uppercase(),
                        color = PS5AccentBlue,
                        fontSize = 32.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = TitilliumFontFamily
                    )
                    Spacer(modifier = Modifier.height(2.dp))
                    Text(
                        text = game.category,
                        color = PS5TextSecondary,
                        fontSize = 10.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = TitilliumFontFamily
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(10.dp))

        // Selected Indicator line
        Box(
            modifier = Modifier
                .width(if (active) 32.dp else 0.dp)
                .height(3.dp)
                .clip(RoundedCornerShape(2.dp))
                .background(PS5TextPrimary)
        )
    }
}

@Composable
fun PS5TrophySummaryBar(
    unlocked: Int,
    total: Int,
    bronze: Int,
    silver: Int,
    gold: Int,
    modifier: Modifier = Modifier
) {
    val progress = if (total > 0) unlocked.toFloat() / total.toFloat() else 0f
    val percent = (progress * 100).toInt()

    Column(modifier = modifier) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Image(
                painter = painterResource(id = R.drawable.ps5_trophies),
                contentDescription = "Trophies",
                modifier = Modifier.size(24.dp)
            )
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = "Trophies Progress",
                fontSize = 14.sp,
                fontWeight = FontWeight.SemiBold,
                color = PS5TextPrimary,
                fontFamily = TitilliumFontFamily
            )
            Spacer(modifier = Modifier.weight(1f))
            Text(
                text = "$percent%",
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                color = PS5TextPrimary,
                fontFamily = TitilliumFontFamily
            )
        }

        Spacer(modifier = Modifier.height(8.dp))

        // Linear Progress Bar
        LinearProgressIndicator(
            progress = { progress },
            modifier = Modifier
                .fillMaxWidth()
                .height(6.dp)
                .clip(RoundedCornerShape(3.dp)),
            color = PS5AccentGlow,
            trackColor = Color.White.copy(alpha = 0.15f)
        )

        Spacer(modifier = Modifier.height(10.dp))

        // Trophy Badges
        Row(
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            TrophyBadge(iconRes = R.drawable.ps5_trophy_gold, count = gold, label = "Gold")
            TrophyBadge(iconRes = R.drawable.ps5_trophy_silver, count = silver, label = "Silver")
            TrophyBadge(iconRes = R.drawable.ps5_trophy_bronze, count = bronze, label = "Bronze")
            Spacer(modifier = Modifier.weight(1f))
            Text(
                text = "$unlocked / $total Total",
                fontSize = 12.sp,
                color = PS5TextSecondary,
                fontFamily = TitilliumFontFamily
            )
        }
    }
}

@Composable
private fun TrophyBadge(
    iconRes: Int,
    count: Int,
    label: String
) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Image(
            painter = painterResource(id = iconRes),
            contentDescription = label,
            modifier = Modifier.size(18.dp)
        )
        Spacer(modifier = Modifier.width(4.dp))
        Text(
            text = count.toString(),
            fontSize = 13.sp,
            fontWeight = FontWeight.Bold,
            color = PS5TextPrimary,
            fontFamily = TitilliumFontFamily
        )
    }
}

@Composable
fun PS5ActivityCard(
    title: String,
    subtitle: String,
    progressText: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .width(220.dp)
            .height(120.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(Color.White.copy(alpha = 0.08f))
            .border(1.dp, Color.White.copy(alpha = 0.15f), RoundedCornerShape(16.dp))
            .clickable { onClick() }
            .padding(16.dp)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            Text(
                text = title,
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                color = PS5TextPrimary,
                fontFamily = TitilliumFontFamily,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text = subtitle,
                fontSize = 12.sp,
                color = PS5TextSecondary,
                fontFamily = TitilliumFontFamily,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis
            )
            Spacer(modifier = Modifier.weight(1f))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text = progressText,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Bold,
                    color = PS5AccentGlow,
                    fontFamily = TitilliumFontFamily
                )
            }
        }
    }
}

@Composable
fun DualSenseButtonPrompts(
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(20.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        ButtonPromptItem(iconRes = R.drawable.ic_dualsense_cross, label = "Select")
        ButtonPromptItem(iconRes = R.drawable.ic_dualsense_circle, label = "Back")
        ButtonPromptItem(iconRes = R.drawable.ic_dualsense_square, label = "Options")
        ButtonPromptItem(iconRes = R.drawable.ic_dualsense_triangle, label = "Search")
    }
}

@Composable
private fun ButtonPromptItem(
    iconRes: Int,
    label: String
) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Image(
            painter = painterResource(id = iconRes),
            contentDescription = label,
            modifier = Modifier.size(18.dp)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = label,
            fontSize = 12.sp,
            color = PS5TextSecondary,
            fontFamily = TitilliumFontFamily,
            fontWeight = FontWeight.SemiBold
        )
    }
}

@Composable
fun PS5BatteryIcon(level: Int, isCharging: Boolean, modifier: Modifier = Modifier) {
    Canvas(modifier = modifier.size(22.dp, 11.dp)) {
        val strokeWidth = 1.dp.toPx()
        val corner = 2.dp.toPx()
        val padding = 1.5.dp.toPx()

        drawRoundRect(
            color = Color.White.copy(alpha = 0.7f),
            style = Stroke(width = strokeWidth),
            cornerRadius = CornerRadius(corner, corner),
            size = Size(size.width - 2.dp.toPx(), size.height)
        )

        drawRect(
            color = Color.White.copy(alpha = 0.7f),
            topLeft = Offset(size.width - 2.dp.toPx(), size.height * 0.25f),
            size = Size(2.dp.toPx(), size.height * 0.5f)
        )

        if (level > 0) {
            val fillWidth = (size.width - 2.dp.toPx() - padding * 2) * (level / 100f)
            drawRoundRect(
                color = if (level < 20) Color.Red else Color.White,
                topLeft = Offset(padding, padding),
                size = Size(fillWidth, size.height - padding * 2),
                cornerRadius = CornerRadius(1.dp.toPx(), 1.dp.toPx())
            )
        }
    }
}

@Composable
fun PS5SystemClock() {
    var currentTime by remember { mutableStateOf("") }

    LaunchedEffect(Unit) {
        while (true) {
            val calendar = java.util.Calendar.getInstance()
            val hour = calendar.get(java.util.Calendar.HOUR_OF_DAY)
            val minute = calendar.get(java.util.Calendar.MINUTE)
            currentTime = String.format(java.util.Locale.getDefault(), "%02d:%02d", hour, minute)
            kotlinx.coroutines.delay(1000)
        }
    }

    Text(
        text = currentTime,
        fontSize = 15.sp,
        fontWeight = FontWeight.Bold,
        color = PS5TextPrimary,
        fontFamily = TitilliumFontFamily
    )
}
