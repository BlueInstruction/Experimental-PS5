import re

# Fix Virtual Pad colors for Clear Glass
with open('app/src/main/java/com/px5/emulator/ui/PS5VirtualPad.kt', 'r') as f:
    content = f.read()

content = re.sub(r'val GLASS_FILL = Color\.White\.copy\(alpha = 0\.05f\)', 'val GLASS_FILL = Color.Transparent', content)
content = re.sub(r'val GLASS_EDGE = Color\.White\.copy\(alpha = 0\.20f\)', 'val GLASS_EDGE = Color.White.copy(alpha = 0.35f)', content)
content = re.sub(r'val GLASS_PRESSED = Color\.White\.copy\(alpha = 0\.35f\)', 'val GLASS_PRESSED = Color.White.copy(alpha = 0.20f)', content)

# Remove the inner border from DPad since we're using Transparent
content = re.sub(r'\.border\(1\.5\.dp, if \(pressed == dir\) GLASS_PRESSED else GLASS_EDGE, RoundedCornerShape\(9\.dp\)\)', '', content)

with open('app/src/main/java/com/px5/emulator/ui/PS5VirtualPad.kt', 'w') as f:
    f.write(content)

# Fix In-Game Menu overlay for Clear Glass
with open('app/src/main/java/com/px5/emulator/ui/EmuScreen.kt', 'r') as f:
    lines = f.readlines()

new_menu = """        // ---- Modern Clear Glass In-Game Menu Overlay ----
        if (showInGameMenu) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.40f))
                    .clickable(
                        interactionSource = remember { androidx.compose.foundation.interaction.MutableInteractionSource() },
                        indication = null
                    ) { showInGameMenu = false },
                contentAlignment = Alignment.Center
            ) {
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    modifier = Modifier
                        .width(340.dp)
                        .clip(RoundedCornerShape(24.dp))
                        .background(Color.White.copy(alpha = 0.08f)) // Glass fill
                        .border(1.5.dp, Color.White.copy(alpha = 0.25f), RoundedCornerShape(24.dp)) // Glass edge
                        .padding(32.dp)
                        .clickable(
                            interactionSource = remember { androidx.compose.foundation.interaction.MutableInteractionSource() },
                            indication = null
                        ) { /* intercept clicks */ }
                ) {
                    Text(
                        text = game?.name ?: path.substringAfterLast('/'),
                        color = Color.White,
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier.padding(bottom = 24.dp)
                    )
                    
                    Button(
                        onClick = { showInGameMenu = false },
                        modifier = Modifier.fillMaxWidth().height(48.dp).padding(bottom = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(12.dp)
                    ) {
                        Text("Resume Game", color = Color.White, fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
                    }
                    
                    Button(
                        onClick = { 
                            showInGameMenu = false
                            padEditing = true
                        },
                        modifier = Modifier.fillMaxWidth().height(48.dp).padding(bottom = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(12.dp)
                    ) {
                        Text("Edit Controls", color = Color.White, fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
                    }

                    Button(
                        onClick = { 
                            showInGameMenu = false
                            showDebugUI = true
                            diagOpen = true
                        },
                        modifier = Modifier.fillMaxWidth().height(48.dp).padding(bottom = 12.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(12.dp)
                    ) {
                        Text("Advanced Diagnostics", color = Color.White, fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
                    }

                    Button(
                        onClick = { onBackClick() },
                        modifier = Modifier.fillMaxWidth().height(48.dp),
                        colors = ButtonDefaults.buttonColors(containerColor = Color.White.copy(alpha = 0.15f)),
                        shape = RoundedCornerShape(12.dp),
                        border = androidx.compose.foundation.BorderStroke(1.dp, Color(0xFFFF5252).copy(alpha = 0.5f))
                    ) {
                        Text("Exit Game", color = Color(0xFFFF5252), fontSize = 15.sp, fontWeight = FontWeight.Bold)
                    }
                }
            }
        }
"""

with open('app/src/main/java/com/px5/emulator/ui/EmuScreen.kt', 'w') as f:
    for i in range(0, 417):
        f.write(lines[i])
    f.write(new_menu)
    for i in range(488, len(lines)):
        f.write(lines[i])

