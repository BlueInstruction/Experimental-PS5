import re

with open('app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt', 'r') as f:
    content = f.read()

# Fix 1: Remove empty `Text(\n)` and trailing `}`
content = re.sub(r'Text\(\s*\n?\s*\)', '', content)
content = re.sub(r'Text\(\s*\n?\s*\}', '}', content)

# Fix 2: Remove dangling `)` near `} else { \n ) \n }`
content = re.sub(r'\} else \{\s*\)\s*\}', '} else {\n        }\n', content)

# Fix 3: Remove dangling modifier lines near 543
content = re.sub(r'modifier = Modifier\.padding\(top = 4\.dp\)\s*\)\s*fontWeight = FontWeight\.Bold,\s*fontFamily = androidx\.compose\.ui\.text\.font\.FontFamily\.Monospace,\s*modifier = Modifier\.padding\(top = 6\.dp\)\s*\)\s*\}', '', content)

with open('app/src/main/java/com/px5/emulator/ui/PS5SettingsScreen.kt', 'w') as f:
    f.write(content)
