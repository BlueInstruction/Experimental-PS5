import re

with open('app/src/main/java/com/px5/emulator/MainActivity.kt', 'r') as f:
    content = f.read()

replacement = """    val realGames by gameViewModel.allGames.collectAsStateWithLifecycle()
    val games = if (realGames.isEmpty()) listOf(
        GameEntity(
            id = "demo_game_ui_test",
            name = "Test Game (UI Preview)",
            titleId = "UI-TEST",
            path = "dummy/path",
            status = "Ready",
            sizeBytes = 25000000000
        )
    ) else realGames"""

content = content.replace('    val games by gameViewModel.allGames.collectAsStateWithLifecycle()', replacement)

with open('app/src/main/java/com/px5/emulator/MainActivity.kt', 'w') as f:
    f.write(content)
