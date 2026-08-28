package com.px5.emulator.ui

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
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
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Notifications
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.ScreenRotation
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.graphics.drawable.toBitmap
import androidx.annotation.DrawableRes
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.painter.Painter
import com.px5.emulator.GameEntity
import java.io.File

@Composable
fun safePainterResource(@DrawableRes id: Int): Painter? {
    val context = LocalContext.current
    val isValid = remember(id, context) {
        try {
            val drawable = context.resources.getDrawable(id, context.theme)
            drawable != null
        } catch (e: Throwable) {
            false
        }
    }
    return if (isValid) painterResource(id = id) else null
}

/**
 * Shared header / tile components.
 *
 * Removed in the honesty pass: the live system clock and the battery
 * meter (the OS already shows both — an emulator shell duplicating them
 * was decoration, and the battery row previously sat next to a fake
 * "DualSense connected" claim). The header now carries only controls
 * that actually do something.
 */

@Composable
fun PS5TopHeader(
    selectedTab: Int, // 0: Games
    onTabSelected: (Int) -> Unit,
    onSearchClick: () -> Unit,
    onSettingsClick: () -> Unit,
    onProfileClick: () -> Unit,
    onRotateClick: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(horizontal = 40.dp, vertical = 16.dp)
            .windowInsetsPadding(WindowInsets.statusBars),
        verticalAlignment = Alignment.CenterVertically
    ) {
        // Navigation Tabs
        Row(
            horizontalArrangement = Arrangement.spacedBy(28.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            HeaderTabItem(
                title = "Games",
                isSelected = selectedTab == 0,
                onClick = { onTabSelected(0) }
            )
        }

        Spacer(modifier = Modifier.weight(1f))

        // System Action Icons
        Row(
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Orientation flip — mirrors the game-hub reference layout: a
            // one-tap portrait/landscape switch, persisted by Px5Settings.
            IconButton(
                onClick = onRotateClick,
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .background(px5Colors().control)
            ) {
                Icon(
                    imageVector = Icons.Default.ScreenRotation,
                    contentDescription = "Rotate screen",
                    tint = px5Colors().text,
                    modifier = Modifier.size(20.dp)
                )
            }

            IconButton(
                onClick = onSearchClick,
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .background(px5Colors().control)
            ) {
                Icon(
                    imageVector = Icons.Default.Search,
                    contentDescription = "Search",
                    tint = px5Colors().text,
                    modifier = Modifier.size(20.dp)
                )
            }

            IconButton(
                onClick = onSettingsClick,
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .background(px5Colors().control)
            ) {
                Icon(
                    imageVector = Icons.Default.Settings,
                    contentDescription = "Settings",
                    tint = px5Colors().text,
                    modifier = Modifier.size(20.dp)
                )
            }

            // Profile avatar opens the Control Center (real telemetry).
            Box(
                modifier = Modifier
                    .size(40.dp)
                    .clip(CircleShape)
                    .border(1.5.dp, PS5AccentBlue, CircleShape)
                    .clickable { onProfileClick() },
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = Icons.Default.Person,
                    contentDescription = "Profile",
                    tint = px5Colors().text,
                    modifier = Modifier.padding(8.dp)
                )
            }
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
            color = if (isSelected) px5Colors().text else px5Colors().textSecondary,
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
fun rememberGameCover(coverPath: String, bannerRes: Int = -1): ImageBitmap? {
    val context = LocalContext.current
    return remember(coverPath) {
        try {
            val f = File(coverPath)
            when {
                coverPath.isNotBlank() && f.isFile -> {
                    android.graphics.BitmapFactory.decodeFile(f.absolutePath)?.asImageBitmap()
                }
                bannerRes != -1 -> {
                    val d = context.resources.getDrawable(bannerRes, context.theme)
                    d?.toBitmap(width = 512, height = 768, config = null)?.asImageBitmap()
                }
                else -> null
            }
        } catch (_: Throwable) {
            null
        }
    }
}

@Composable
fun PX5GameCardTile(
    game: GameEntity,
    isSelected: Boolean,
    isAddTile: Boolean = false,
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
                .size(120.dp, 170.dp)
                .scale(tileScale)
                .clip(RoundedCornerShape(12.dp))
                .background(
                    if (active) androidx.compose.ui.graphics.Brush.linearGradient(
                        colors = listOf(Color(0xFF2E3D59), Color(0xFF1E2838))
                    ) else androidx.compose.ui.graphics.Brush.linearGradient(
                        colors = listOf(Color(0xFF1A2230), Color(0xFF121822))
                    )
                )
                .border(
                    width = if (active) 2.5.dp else 1.dp,
                    color = if (active) px5Colors().text else px5Colors().hairline,
                    shape = RoundedCornerShape(12.dp)
                )
                .clickable(
                    interactionSource = interactionSource,
                    indication = null,
                    onClick = onClick
                ),
            contentAlignment = Alignment.Center
        ) {
            if (isAddTile) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center
                ) {
                    Icon(
                        imageVector = Icons.Default.Add,
                        contentDescription = "Add Game",
                        tint = px5Colors().text,
                        modifier = Modifier.size(42.dp)
                    )
                    Spacer(Modifier.height(8.dp))
                    Text(
                        text = "Add Game",
                        fontSize = 12.sp,
                        fontWeight = FontWeight.SemiBold,
                        color = px5Colors().textSecondary,
                        fontFamily = TitilliumFontFamily
                    )
                }
            } else {
                val cover = rememberGameCover(game.coverPath)
                if (cover != null) {
                    Image(
                        bitmap = cover,
                        contentDescription = game.name,
                        contentScale = ContentScale.Crop,
                        modifier = Modifier.fillMaxSize()
                    )
                } else {
                    Column(
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.Center,
                        modifier = Modifier.padding(10.dp)
                    ) {
                        Text(
                            text = game.name.take(2).uppercase(),
                            color = PS5AccentBlue,
                            fontSize = 32.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = TitilliumFontFamily,
                            textAlign = TextAlign.Center
                        )
                        Spacer(modifier = Modifier.height(4.dp))
                        Text(
                            text = game.format,
                            color = px5Colors().textSecondary,
                            fontSize = 10.sp,
                            fontWeight = FontWeight.Bold,
                            fontFamily = TitilliumFontFamily
                        )
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(8.dp))

        Text(
            text = if (isAddTile) " " else game.name,
            color = if (active) px5Colors().text else px5Colors().textSecondary,
            fontSize = 12.sp,
            fontWeight = FontWeight.SemiBold,
            fontFamily = TitilliumFontFamily,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.width(120.dp),
            textAlign = TextAlign.Center
        )
    }
}

/** Compact tile used by the portrait grid. */
@Composable
fun PX5GameGridTile(
    game: GameEntity,
    isAddTile: Boolean = false,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = modifier
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .aspectRatio(0.7f)
                .clip(RoundedCornerShape(12.dp))
                .background(
                    androidx.compose.ui.graphics.Brush.linearGradient(
                        colors = listOf(Color(0xFF1A2230), Color(0xFF121822))
                    )
                )
                .border(
                    width = 1.dp,
                    color = px5Colors().hairline,
                    shape = RoundedCornerShape(12.dp)
                )
                .clickable(onClick = onClick),
            contentAlignment = Alignment.Center
        ) {
            if (isAddTile) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center
                ) {
                    Icon(
                        imageVector = Icons.Default.Add,
                        contentDescription = "Add Game",
                        tint = px5Colors().text,
                        modifier = Modifier.size(36.dp)
                    )
                    Spacer(Modifier.height(6.dp))
                    Text(
                        text = "Add",
                        fontSize = 12.sp,
                        color = px5Colors().textSecondary,
                        fontFamily = TitilliumFontFamily
                    )
                }
            } else {
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
                        color = PS5AccentBlue,
                        fontSize = 28.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = TitilliumFontFamily
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(6.dp))

        Text(
            text = if (isAddTile) " " else game.name,
            color = px5Colors().textSecondary,
            fontSize = 12.sp,
            fontFamily = TitilliumFontFamily,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.fillMaxWidth(),
            textAlign = TextAlign.Center
        )
    }
}
