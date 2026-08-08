package com.px5.emulator.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
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
import com.px5.emulator.R

@Composable
fun PS5StoreScreen(
    onAddGameClick: () -> Unit,
    onBackClick: () -> Unit,
    onGameSelected: (String) -> Unit
) {
    val storeFeaturedGames = listOf(
        GameEntity(
            id = "store_1",
            name = "Final Fantasy XVI",
            path = "/sdcard/PX5/Games/FFXVI.elf",
            category = "PS5 Store",
            developer = "Square Enix",
            sizeGb = "90.5 GB"
        ),
        GameEntity(
            id = "store_2",
            name = "Horizon Forbidden West",
            path = "/sdcard/PX5/Games/HorizonFW.elf",
            category = "PS5 Store",
            developer = "Guerrilla Games",
            sizeGb = "98.1 GB"
        ),
        GameEntity(
            id = "store_3",
            name = "Returnal",
            path = "/sdcard/PX5/Games/Returnal.elf",
            category = "PS5 Store",
            developer = "Housemarque",
            sizeGb = "56.2 GB"
        ),
        GameEntity(
            id = "store_4",
            name = "Death Stranding DC",
            path = "/sdcard/PX5/Games/DeathStranding.elf",
            category = "PS5 Store",
            developer = "Kojima Productions",
            sizeGb = "68.0 GB"
        )
    )

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(PS5DarkBackground)
    ) {
        // Store Backdrop
        Image(
            painter = painterResource(id = R.drawable.ps5_store_background),
            contentDescription = "Store Background",
            contentScale = ContentScale.Crop,
            modifier = Modifier.fillMaxSize(),
            alpha = 0.45f
        )

        // Gradient overlay
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(
                    Brush.verticalGradient(
                        colors = listOf(
                            Color.Black.copy(alpha = 0.4f),
                            Color(0xFF0B0E14).copy(alpha = 0.95f)
                        )
                    )
                )
        )

        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(32.dp)
                .windowInsetsPadding(WindowInsets.statusBars)
        ) {
            // Header Row
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(
                    onClick = onBackClick,
                    modifier = Modifier
                        .size(44.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.1f))
                ) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Back",
                        tint = PS5TextPrimary
                    )
                }

                Spacer(modifier = Modifier.width(16.dp))

                Image(
                    painter = painterResource(id = R.drawable.ps5_playstation_store),
                    contentDescription = "PlayStation Store",
                    modifier = Modifier.size(36.dp)
                )

                Spacer(modifier = Modifier.width(12.dp))

                Text(
                    text = "PlayStation Store",
                    fontSize = 28.sp,
                    fontWeight = FontWeight.Bold,
                    color = PS5TextPrimary,
                    fontFamily = TitilliumFontFamily
                )

                Spacer(modifier = Modifier.weight(1f))

                // Load Game / Add Application Button
                Button(
                    onClick = onAddGameClick,
                    colors = ButtonDefaults.buttonColors(containerColor = PS5AccentBlue, contentColor = Color.White),
                    shape = RoundedCornerShape(24.dp),
                    contentPadding = PaddingValues(horizontal = 24.dp, vertical = 12.dp)
                ) {
                    Icon(imageVector = Icons.Default.Add, contentDescription = "Add", modifier = Modifier.size(18.dp))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "Install Game (.elf)", fontWeight = FontWeight.Bold, fontFamily = TitilliumFontFamily)
                }
            }

            Spacer(modifier = Modifier.height(28.dp))

            Text(
                text = "Featured Games & Subsystems",
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
                color = PS5TextPrimary,
                fontFamily = TitilliumFontFamily
            )

            Spacer(modifier = Modifier.height(16.dp))

            LazyVerticalGrid(
                columns = GridCells.Adaptive(minSize = 220.dp),
                horizontalArrangement = Arrangement.spacedBy(20.dp),
                verticalArrangement = Arrangement.spacedBy(20.dp),
                modifier = Modifier.fillMaxSize()
            ) {
                items(storeFeaturedGames) { game ->
                    StoreGameCard(
                        game = game,
                        onClick = { onGameSelected(game.path) }
                    )
                }
            }
        }
    }
}

@Composable
private fun StoreGameCard(
    game: GameEntity,
    onClick: () -> Unit
) {
    Card(
        colors = CardDefaults.cardColors(containerColor = Color.White.copy(alpha = 0.08f)),
        shape = RoundedCornerShape(18.dp),
        modifier = Modifier
            .fillMaxWidth()
            .border(1.dp, Color.White.copy(alpha = 0.15f), RoundedCornerShape(18.dp))
    ) {
        Column(modifier = Modifier.padding(20.dp)) {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(110.dp)
                    .clip(RoundedCornerShape(12.dp))
                    .background(
                        Brush.linearGradient(
                            colors = listOf(PS5AccentBlue.copy(alpha = 0.6f), Color(0xFF1E2838))
                        )
                    ),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = game.name,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    color = PS5TextPrimary,
                    fontFamily = TitilliumFontFamily
                )
            }

            Spacer(modifier = Modifier.height(14.dp))

            Text(
                text = game.developer,
                fontSize = 12.sp,
                color = PS5TextSecondary,
                fontFamily = TitilliumFontFamily
            )

            Spacer(modifier = Modifier.height(4.dp))

            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = game.sizeGb,
                    fontSize = 12.sp,
                    color = PS5AccentGlow,
                    fontFamily = TitilliumFontFamily,
                    fontWeight = FontWeight.Bold
                )

                Spacer(modifier = Modifier.weight(1f))

                Button(
                    onClick = onClick,
                    colors = ButtonDefaults.buttonColors(containerColor = Color.White, contentColor = Color.Black),
                    shape = RoundedCornerShape(16.dp),
                    contentPadding = PaddingValues(horizontal = 16.dp, vertical = 6.dp)
                ) {
                    Text(text = "Get", fontWeight = FontWeight.Bold, fontFamily = TitilliumFontFamily)
                }
            }
        }
    }
}
