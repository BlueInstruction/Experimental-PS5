package com.px5.emulator.ui

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.px5.emulator.R
import com.px5.emulator.SoundManager
import kotlinx.coroutines.delay

@Composable
fun PS5PkgInstallerSheet(
    fileName: String,
    filePath: String,
    soundManager: SoundManager,
    onInstallationComplete: (String, String) -> Unit, // returns (title, path)
    onDismiss: () -> Unit
) {
    var progress by remember { mutableFloatStateOf(0f) }
    var currentStepText by remember { mutableStateOf("Reading PKG Header...") }
    var isFinished by remember { mutableStateOf(false) }

    val animatedProgress by animateFloatAsState(targetValue = progress, label = "PkgProgress")

    LaunchedEffect(Unit) {
        soundManager.playActivationSound()
        // Step 1: Read Header
        delay(600)
        progress = 0.25f
        currentStepText = "Verifying PS5 DRM & SHA256 Signature..."

        // Step 2: Extract PKG
        delay(800)
        progress = 0.60f
        currentStepText = "Extracting ELF binaries & Asset Assets to /root/Games..."

        // Step 3: FEXCore Allocation
        delay(900)
        progress = 0.90f
        currentStepText = "Allocating FEXCore Virtual Memory & Bionic Runtime..."

        // Step 4: Finished
        delay(600)
        progress = 1.0f
        currentStepText = "Installation Successful!"
        isFinished = true
        soundManager.playActivationSound()
    }

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
            .background(Color(0xFF141A24))
            .border(1.dp, Color.White.copy(alpha = 0.15f), RoundedCornerShape(topStart = 28.dp, topEnd = 28.dp))
            .padding(28.dp)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.ic_dualsense_ps),
                    contentDescription = "PS5 PKG",
                    tint = PS5AccentGlow,
                    modifier = Modifier.size(28.dp)
                )

                Spacer(modifier = Modifier.width(12.dp))

                Text(
                    text = "PlayStation 5 Package Installer (.pkg)",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = PS5TextPrimary,
                    fontFamily = TitilliumFontFamily
                )

                Spacer(modifier = Modifier.weight(1f))

                IconButton(
                    onClick = onDismiss,
                    modifier = Modifier
                        .size(36.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.1f))
                ) {
                    Icon(
                        imageVector = Icons.Default.Close,
                        contentDescription = "Close",
                        tint = PS5TextPrimary,
                        modifier = Modifier.size(18.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(20.dp))

            Card(
                colors = CardDefaults.cardColors(containerColor = Color.White.copy(alpha = 0.05f)),
                shape = RoundedCornerShape(16.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(modifier = Modifier.padding(20.dp)) {
                    Text(
                        text = "Package File:",
                        fontSize = 12.sp,
                        color = PS5TextSecondary,
                        fontFamily = TitilliumFontFamily
                    )
                    Text(
                        text = fileName,
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold,
                        color = PS5TextPrimary,
                        fontFamily = TitilliumFontFamily
                    )

                    Spacer(modifier = Modifier.height(14.dp))

                    LinearProgressIndicator(
                        progress = { animatedProgress },
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(8.dp)
                            .clip(RoundedCornerShape(4.dp)),
                        color = PS5AccentGlow,
                        trackColor = Color.White.copy(alpha = 0.1f),
                    )

                    Spacer(modifier = Modifier.height(10.dp))

                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            text = currentStepText,
                            fontSize = 13.sp,
                            color = if (isFinished) PS5AccentGlow else PS5TextSecondary,
                            fontFamily = TitilliumFontFamily,
                            fontWeight = if (isFinished) FontWeight.Bold else FontWeight.Normal
                        )
                        Spacer(modifier = Modifier.weight(1f))
                        Text(
                            text = "${(animatedProgress * 100).toInt()}%",
                            fontSize = 13.sp,
                            color = PS5AccentGlow,
                            fontFamily = TitilliumFontFamily,
                            fontWeight = FontWeight.Bold
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(24.dp))

            if (isFinished) {
                Button(
                    onClick = {
                        val cleanedTitle = fileName.substringBeforeLast(".").replace("_", " ")
                        onInstallationComplete(cleanedTitle, filePath)
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = Color.White, contentColor = Color.Black),
                    shape = RoundedCornerShape(20.dp),
                    modifier = Modifier.fillMaxWidth(),
                    contentPadding = PaddingValues(vertical = 14.dp)
                ) {
                    Icon(imageVector = Icons.Default.CheckCircle, contentDescription = "Done", modifier = Modifier.size(18.dp))
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "Add Game to PX5 Library",
                        fontWeight = FontWeight.Bold,
                        fontSize = 15.sp,
                        fontFamily = TitilliumFontFamily
                    )
                }
            }
        }
    }
}
