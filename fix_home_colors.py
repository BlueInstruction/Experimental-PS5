import re

with open('app/src/main/java/com/px5/emulator/ui/PS5HomeScreen.kt', 'r') as f:
    content = f.read()

# Replace the hardcoded colors with px5Colors() equivalents
content = re.sub(r'val darkBackground = androidx\.compose\.ui\.graphics\.Color\([^)]+\)', 'val darkBackground = px5Colors().background', content)
content = re.sub(r'val surfaceColor = androidx\.compose\.ui\.graphics\.Color\([^)]+\)', 'val surfaceColor = px5Colors().surface', content)
content = re.sub(r'val textColor = androidx\.compose\.ui\.graphics\.Color\([^)]+\)', 'val textColor = px5Colors().text', content)
content = re.sub(r'val textSecondary = androidx\.compose\.ui\.graphics\.Color\([^)]+\)', 'val textSecondary = px5Colors().textSecondary', content)
content = re.sub(r'val accentColor = androidx\.compose\.ui\.graphics\.Color\([^)]+\)', 'val accentColor = px5Colors().accent', content)

with open('app/src/main/java/com/px5/emulator/ui/PS5HomeScreen.kt', 'w') as f:
    f.write(content)
