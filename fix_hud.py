with open('app/src/main/java/com/px5/emulator/ui/EmuScreen.kt', 'r') as f:
    lines = f.readlines()

new_hud = """
        // ---- Minimal HUD ----
        if (!showInGameMenu && !showDebugUI) {
            Row(modifier = Modifier.align(Alignment.TopEnd).padding(16.dp)) {
                if (showFps && fpsText.isNotEmpty()) {
                    Text(fpsText, color = Color(0xFF69F0AE), fontSize = 12.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(end = 8.dp))
                }
                if (showFrametime && frametimeText.isNotEmpty()) {
                    Text(frametimeText, color = Color(0xFF7DD3FC), fontSize = 12.sp, fontWeight = FontWeight.Bold)
                }
            }
            
            // Invisible top-left touch zone for devices without physical back button
            Box(
                modifier = Modifier
                    .align(Alignment.TopStart)
                    .size(80.dp)
                    .clickable(
                        interactionSource = remember { androidx.compose.foundation.interaction.MutableInteractionSource() },
                        indication = null
                    ) { showInGameMenu = true }
            )
        }

        // ---- Modern In-Game Menu Overlay ----
        if (showInGameMenu) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.75f))
                    .clickable(
                        interactionSource = remember { androidx.compose.foundation.interaction.MutableInteractionSource() },
                        indication = null
                    ) { showInGameMenu = false },
                contentAlignment = Alignment.Center
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    modifier = Modifier.width(320.dp).clickable(
                        interactionSource = remember { androidx.compose.foundation.interaction.MutableInteractionSource() },
                        indication = null
                    ) { /* intercept clicks */ }
                ) {
                    Text(
                        text = game?.name ?: path.substringAfterLast('/'),
                        color = Color.White,
                        fontSize = 22.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier.padding(bottom = 32.dp)
                    )
                    
                    Button(
                        onClick = { showInGameMenu = false },
                        modifier = Modifier.fillMaxWidth().height(48.dp).padding(bottom = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text("Resume Game", color = Color.White, fontSize = 16.sp)
                    }
                    
                    Button(
                        onClick = { 
                            showInGameMenu = false
                            padEditing = true
                        },
                        modifier = Modifier.fillMaxWidth().height(48.dp).padding(bottom = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text("Edit Virtual Controls", color = Color.White, fontSize = 16.sp)
                    }

                    Button(
                        onClick = { 
                            showInGameMenu = false
                            showDebugUI = true
                            diagOpen = true
                        },
                        modifier = Modifier.fillMaxWidth().height(48.dp).padding(bottom = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text("Advanced Diagnostics", color = Color.White, fontSize = 16.sp)
                    }

                    Button(
                        onClick = { onBackClick() },
                        modifier = Modifier.fillMaxWidth().height(48.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFE53935)),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text("Exit Game", color = Color.White, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                    }
                }
            }
        }
        
        // ---- Advanced / Messy Debug UI (Only shown if toggled) ----
        if (showDebugUI) {
            Box(modifier = Modifier.fillMaxSize().background(Color.Black.copy(alpha=0.4f))) {
                IconButton(
                    onClick = { showDebugUI = false },
                    modifier = Modifier.align(Alignment.TopEnd).padding(16.dp).size(36.dp).clip(CircleShape).background(Color.White.copy(alpha=0.2f)).zIndex(10f)
                ) {
                    Icon(Icons.Default.Close, contentDescription = "Close Debug", tint = Color.White)
                }
"""

with open('app/src/main/java/com/px5/emulator/ui/EmuScreen.kt', 'w') as f:
    for i in range(0, 388):
        f.write(lines[i])
    
    f.write(new_hud)
    
    for i in range(388, 783):
        # Indent the old UI code so it falls inside the `if (showDebugUI)` Box
        f.write("                " + lines[i])
    
    # Close the showDebugUI Box
    f.write("            }\n")
    f.write("        }\n")
    
    for i in range(783, len(lines)):
        f.write(lines[i])

