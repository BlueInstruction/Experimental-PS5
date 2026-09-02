import re

with open('app/src/main/java/com/px5/emulator/ui/EmuScreen.kt', 'r') as f:
    content = f.read()

# Add new state variables
state_vars = """    var showInGameMenu by remember { mutableStateOf(false) }
    var showDebugUI by remember { mutableStateOf(false) }
    var padEditing by remember { mutableStateOf(false) }"""
content = re.sub(r'var padEditing by remember \{ mutableStateOf\(false\) \}', state_vars, content)

# Inject BackHandler and new imports if needed
back_handler_import = "import androidx.activity.compose.BackHandler\n"
if "import androidx.activity.compose.BackHandler" not in content:
    content = content.replace("import androidx.compose.ui.Modifier", back_handler_import + "import androidx.compose.ui.Modifier")

# Inject BackHandler into the Compose function
back_handler_code = """
    BackHandler(enabled = !showInGameMenu) {
        showInGameMenu = true
    }
"""
content = re.sub(r'(val showFrametime.*?)\n', r'\1\n' + back_handler_code, content)

# Now rewrite from overlay layer 2 down to the end of the Box
overlay_pattern = re.compile(r'// ---- overlay layer 2.*?^    }$', re.MULTILINE | re.DOTALL)

new_overlay_code = """        // ---- Minimal HUD (FPS) ----
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
            Box(modifier = Modifier.align(Alignment.TopStart).size(64.dp).clickable(
                interactionSource = remember { androidx.compose.foundation.interaction.MutableInteractionSource() },
                indication = null
            ) { showInGameMenu = true })
        }

        // ---- Modern In-Game Menu Overlay ----
        if (showInGameMenu) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.65f))
                    .clickable(
                        interactionSource = remember { androidx.compose.foundation.interaction.MutableInteractionSource() },
                        indication = null
                    ) { showInGameMenu = false },
                contentAlignment = Alignment.Center
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    modifier = Modifier.width(300.dp)
                ) {
                    Text(
                        text = game?.name ?: "Running",
                        color = Color.White,
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier.padding(bottom = 24.dp)
                    )
                    
                    Button(
                        onClick = { showInGameMenu = false },
                        modifier = Modifier.fillMaxWidth().padding(bottom = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text("Resume Game", color = Color.White)
                    }
                    
                    Button(
                        onClick = { 
                            showInGameMenu = false
                            padEditing = !padEditing
                        },
                        modifier = Modifier.fillMaxWidth().padding(bottom = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text("Edit Controls", color = Color.White)
                    }

                    Button(
                        onClick = { 
                            showInGameMenu = false
                            showDebugUI = true
                            diagOpen = true
                        },
                        modifier = Modifier.fillMaxWidth().padding(bottom = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text("Advanced Diagnostics", color = Color.White)
                    }

                    Button(
                        onClick = { onBackClick() },
                        modifier = Modifier.fillMaxWidth(),
                        colors = ButtonDefaults.buttonColors(containerColor = Color(0xFFFF4D4D).copy(alpha = 0.8f)),
                        shape = RoundedCornerShape(8.dp)
                    ) {
                        Text("Exit Game", color = Color.White)
                    }
                }
            }
        }
        
        // ---- Advanced / Messy Debug UI (Only shown if toggled) ----
        if (showDebugUI) {
            Box(modifier = Modifier.fillMaxSize().background(Color.Black.copy(alpha=0.4f))) {
                IconButton(
                    onClick = { showDebugUI = false },
                    modifier = Modifier.align(Alignment.TopEnd).padding(16.dp).size(36.dp).clip(CircleShape).background(Color.White.copy(alpha=0.2f))
                ) {
                    Icon(Icons.Default.Close, contentDescription = "Close Debug", tint = Color.White)
                }
"""

with open('app/src/main/java/com/px5/emulator/ui/EmuScreen.kt', 'w') as f:
    f.write(content)
