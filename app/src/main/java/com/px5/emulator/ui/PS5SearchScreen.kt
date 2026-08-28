package com.px5.emulator.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.GameEntity

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PS5SearchScreen(
    games: List<GameEntity>,
    onGameSelected: (GameEntity) -> Unit,
    onBackClick: () -> Unit
) {
    var searchQuery by remember { mutableStateOf("") }
    val filteredGames = remember(searchQuery, games) {
        if (searchQuery.isBlank()) games
        else games.filter {
            it.name.contains(searchQuery, ignoreCase = true) ||
                    it.titleId.contains(searchQuery, ignoreCase = true)
        }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(PS5DarkBackground)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(32.dp)
                .windowInsetsPadding(WindowInsets.statusBars)
        ) {
            // Header Search Input Row
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(
                    onClick = onBackClick,
                    modifier = Modifier
                        .size(44.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.08f))
                ) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Back",
                        tint = PS5TextPrimary
                    )
                }

                Spacer(modifier = Modifier.width(16.dp))

                TextField(
                    value = searchQuery,
                    onValueChange = { searchQuery = it },
                    placeholder = {
                        Text(
                            text = "Search your games...",
                            color = PS5TextSecondary,
                            fontFamily = TitilliumFontFamily
                        )
                    },
                    leadingIcon = {
                        Icon(imageVector = Icons.Default.Search, contentDescription = "Search", tint = PS5TextPrimary)
                    },
                    trailingIcon = if (searchQuery.isNotEmpty()) {
                        {
                            IconButton(onClick = { searchQuery = "" }) {
                                Icon(imageVector = Icons.Default.Close, contentDescription = "Clear", tint = PS5TextPrimary)
                            }
                        }
                    } else null,
                    singleLine = true,
                    colors = TextFieldDefaults.colors(
                        focusedContainerColor = Color.White.copy(alpha = 0.12f),
                        unfocusedContainerColor = Color.White.copy(alpha = 0.08f),
                        focusedIndicatorColor = Color.Transparent,
                        unfocusedIndicatorColor = Color.Transparent,
                        focusedTextColor = PS5TextPrimary,
                        unfocusedTextColor = PS5TextPrimary
                    ),
                    shape = RoundedCornerShape(24.dp),
                    modifier = Modifier.weight(1f)
                )
            }

            Spacer(modifier = Modifier.height(28.dp))

            Text(
                text = "Results (${filteredGames.size})",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                color = PS5TextPrimary,
                fontFamily = TitilliumFontFamily
            )

            Spacer(modifier = Modifier.height(16.dp))

            if (filteredGames.isEmpty()) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "No matching games found.",
                        color = PS5TextSecondary,
                        fontSize = 16.sp,
                        fontFamily = TitilliumFontFamily
                    )
                }
            } else {
                LazyColumn(
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                    modifier = Modifier.fillMaxSize()
                ) {
                    items(filteredGames) { game ->
                        SearchResultCard(
                            game = game,
                            onClick = { onGameSelected(game) }
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun SearchResultCard(
    game: GameEntity,
    onClick: () -> Unit
) {
    Card(
        colors = CardDefaults.cardColors(containerColor = Color.White.copy(alpha = 0.06f)),
        shape = RoundedCornerShape(16.dp),
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(52.dp)
                    .clip(RoundedCornerShape(12.dp))
                    .background(PS5AccentBlue.copy(alpha = 0.3f)),
                contentAlignment = Alignment.Center
            ) {
                val cover = rememberGameCover(game.coverPath)
                if (cover != null) {
                    androidx.compose.foundation.Image(
                        bitmap = cover,
                        contentDescription = null,
                        contentScale = androidx.compose.ui.layout.ContentScale.Crop,
                        modifier = Modifier.fillMaxSize()
                    )
                } else {
                    Text(
                        text = game.name.take(2).uppercase(),
                        color = PS5TextPrimary,
                        fontWeight = FontWeight.Bold,
                        fontSize = 20.sp,
                        fontFamily = TitilliumFontFamily
                    )
                }
            }

            Spacer(modifier = Modifier.width(16.dp))

            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = game.name,
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    color = PS5TextPrimary,
                    fontFamily = TitilliumFontFamily
                )
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = buildString {
                        append(game.format)
                        if (game.titleId.isNotBlank()) append(" • ").append(game.titleId)
                        append(" • ").append(formatBytes(game.sizeBytes))
                    },
                    fontSize = 12.sp,
                    color = PS5TextSecondary,
                    fontFamily = TitilliumFontFamily
                )
            }

            Button(
                onClick = onClick,
                colors = ButtonDefaults.buttonColors(containerColor = Color.White, contentColor = Color.Black),
                shape = RoundedCornerShape(20.dp),
                contentPadding = PaddingValues(horizontal = 20.dp, vertical = 8.dp)
            ) {
                Text(text = "Open", fontWeight = FontWeight.Bold, fontFamily = TitilliumFontFamily)
            }
        }
    }
}
