import re

with open('app/src/main/java/com/px5/emulator/ui/EmuScreen.kt', 'r') as f:
    content = f.read()

imports = """import androidx.compose.foundation.clickable
import androidx.compose.ui.zIndex
import androidx.compose.material.icons.filled.Close
import androidx.activity.compose.BackHandler
"""

# Insert imports after package definition
content = re.sub(r'package com.px5.emulator.ui\n', r'package com.px5.emulator.ui\n' + imports, content)

with open('app/src/main/java/com/px5/emulator/ui/EmuScreen.kt', 'w') as f:
    f.write(content)
