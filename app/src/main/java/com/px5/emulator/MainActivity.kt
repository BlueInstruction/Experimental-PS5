package com.px5.emulator
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsFocusedAsState
import androidx.compose.foundation.interaction.collectIsHoveredAsState
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.Image
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Favorite
import androidx.compose.material.icons.filled.FavoriteBorder
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.foundation.Canvas
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

import com.px5.emulator.core.FexCoreWrapper

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        try {
            val fexCore = FexCoreWrapper()
            val initSuccess = fexCore.initializeFexCore()
            android.util.Log.i("PX5_JNI", "FEXCore init status: $initSuccess - ${fexCore.stringFromJNI()}")
        } catch (e: Exception) {
            android.util.Log.e("PX5_JNI", "Failed to initialize FEXCore: ${e.message}")
        }
        
        enableEdgeToEdge()
        setContent {
            PX5Theme {
                AppNavigation()
            }
        }
    }
}




@Composable
fun PX5Theme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = darkColorScheme(
            background = Color.Black,
            surface = Color(0xFF1E1E1E),
            onBackground = Color.White,
            onSurface = Color.White
        ),
        content = content
    )
}

@Composable
fun AppNavigation(gameViewModel: GameViewModel = viewModel()) {
    val games by gameViewModel.allGames.collectAsStateWithLifecycle()
    val navController = rememberNavController()
    val drawerState = rememberDrawerState(initialValue = DrawerValue.Closed)
    val coroutineScope = rememberCoroutineScope()

    ModalNavigationDrawer(
        drawerState = drawerState,
        drawerContent = {
            ModalDrawerSheet(
                drawerContainerColor = Color(0xFF121212),
                modifier = Modifier.width(300.dp)
            ) {
                Spacer(Modifier.height(24.dp))
                Text("Control Center", color = Color.White, fontSize = 24.sp, fontWeight = FontWeight.Bold, modifier = Modifier.padding(16.dp))
                HorizontalDivider(color = Color.DarkGray)
                NavigationDrawerItem(
                    label = { Text("Home", color = Color.White) },
                    selected = false,
                    onClick = {
                        coroutineScope.launch { drawerState.close() }
                        navController.navigate("home") { popUpTo(0) }
                    },
                    modifier = Modifier.padding(horizontal = 12.dp, vertical = 4.dp),
                    colors = NavigationDrawerItemDefaults.colors(unselectedContainerColor = Color.Transparent)
                )
                NavigationDrawerItem(
                    label = { Text("App Library", color = Color.White) },
                    selected = false,
                    onClick = {
                        coroutineScope.launch { drawerState.close() }
                        navController.navigate("library")
                    },
                    modifier = Modifier.padding(horizontal = 12.dp, vertical = 4.dp),
                    colors = NavigationDrawerItemDefaults.colors(unselectedContainerColor = Color.Transparent)
                )
                NavigationDrawerItem(
                    label = { Text("Settings", color = Color.White) },
                    selected = false,
                    onClick = {
                        coroutineScope.launch { drawerState.close() }
                        navController.navigate("settings")
                    },
                    modifier = Modifier.padding(horizontal = 12.dp, vertical = 4.dp),
                    colors = NavigationDrawerItemDefaults.colors(unselectedContainerColor = Color.Transparent)
                )
            }
        }
    ) {
        NavHost(navController = navController, startDestination = "home") {
            composable("home") {
                HomeScreen(
                    games = games,
                    gameViewModel = gameViewModel,
                    onGameSelected = { path ->
                        navController.navigate("emulation?path=$path")
                    },
                    onSettingsClick = {
                        navController.navigate("settings")
                    },
                    onOpenDrawer = {
                        coroutineScope.launch { drawerState.open() }
                    },
                    onLibraryClick = {
                        navController.navigate("library")
                    }
                )
            }
            composable("library") {
                AppLibraryScreen(
                    games = games,
                    gameViewModel = gameViewModel,
                    onBackClick = { navController.popBackStack() },
                    onGameSelected = { path -> navController.navigate("emulation?path=$path") }
                )
            }
            composable("settings") {
                SettingsScreen(
                    onBackClick = {
                        navController.popBackStack()
                    }
                )
            }
            composable("emulation?path={path}") { backStackEntry ->
                val path = backStackEntry.arguments?.getString("path") ?: ""
                EmulationScreen(path = path)
            }
        }
    }
}

@Composable
fun SettingsScreen(onBackClick: () -> Unit) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(24.dp)
                    .windowInsetsPadding(WindowInsets.statusBars),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(onClick = onBackClick) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Back",
                        tint = Color.White
                    )
                }
                Spacer(modifier = Modifier.width(16.dp))
                Text(
                    text = "Settings",
                    fontSize = 28.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color.White
                )
            }
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(24.dp),
                contentAlignment = Alignment.Center
            ) {
                Text("Settings panel coming soon.", color = Color.Gray, fontSize = 16.sp)
            }
        }
    }
}

@Composable
fun AppLibraryScreen(
    games: List<GameEntity>,
    gameViewModel: GameViewModel,
    onBackClick: () -> Unit,
    onGameSelected: (String) -> Unit
) {
    var showFavoritesOnly by remember { mutableStateOf(false) }
    val displayedGames = if (showFavoritesOnly) games.filter { it.isFavorite } else games

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFF121212))
    ) {
        if (displayedGames.isEmpty() && games.isEmpty()) {
            Image(
                painter = painterResource(id = R.drawable.empty_state_games_1786087429410),
                contentDescription = "Background",
                contentScale = ContentScale.Crop,
                modifier = Modifier.fillMaxSize(),
                alpha = 0.5f
            )
        }

        Column(modifier = Modifier.fillMaxSize()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(24.dp)
                    .windowInsetsPadding(WindowInsets.statusBars),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(onClick = onBackClick) {
                    Icon(imageVector = Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back", tint = Color.White)
                }
                Spacer(modifier = Modifier.width(16.dp))
                Text("App Library", fontSize = 28.sp, fontWeight = FontWeight.Bold, color = Color.White)
                
                Spacer(modifier = Modifier.weight(1f))
                
                FilterChip(
                    selected = showFavoritesOnly,
                    onClick = { showFavoritesOnly = !showFavoritesOnly },
                    label = { Text("Favorites") },
                    leadingIcon = if (showFavoritesOnly) {
                        { Icon(imageVector = Icons.Default.Favorite, contentDescription = "Favorites", modifier = Modifier.size(18.dp)) }
                    } else {
                        { Icon(imageVector = Icons.Default.FavoriteBorder, contentDescription = "Favorites", modifier = Modifier.size(18.dp)) }
                    }
                )
            }
            
            if (displayedGames.isEmpty()) {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = if (showFavoritesOnly) "No favorite games found" else "No games found",
                        color = Color.White,
                        fontSize = 24.sp,
                        fontWeight = FontWeight.Light
                    )
                }
            } else {
                LazyVerticalGrid(
                    columns = GridCells.Adaptive(minSize = 160.dp),
                    contentPadding = PaddingValues(24.dp),
                    horizontalArrangement = Arrangement.spacedBy(24.dp),
                    verticalArrangement = Arrangement.spacedBy(24.dp),
                    modifier = Modifier.fillMaxSize()
                ) {
                    items(displayedGames) { game ->
                        PX5GameCard(
                            game = game,
                            onClick = { onGameSelected(game.path) },
                            onFavoriteClick = { gameViewModel.toggleFavorite(game.id, !game.isFavorite) },
                            onHideClick = { gameViewModel.delete(game.id) }
                        )
                    }
                }
            }
        }
    }
}

@Composable
fun HomeScreen(
    games: List<GameEntity>,
    gameViewModel: GameViewModel,
    onGameSelected: (String) -> Unit,
    onSettingsClick: () -> Unit,
    onOpenDrawer: () -> Unit,
    onLibraryClick: () -> Unit
) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    var statusText by remember { mutableStateOf("Ready — Uninitialized") }
    var selectedIndex by remember { mutableStateOf(0) }
    var batteryLevel by remember { mutableStateOf(-1) }
    var batteryIsCharging by remember { mutableStateOf(false) }

    DisposableEffect(context) {
        val batteryReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                val level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
                val scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
                if (level != -1 && scale != -1) {
                    batteryLevel = (level * 100 / scale.toFloat()).toInt()
                }
                val status = intent.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
                batteryIsCharging = status == BatteryManager.BATTERY_STATUS_CHARGING ||
                        status == BatteryManager.BATTERY_STATUS_FULL
            }
        }
        context.registerReceiver(batteryReceiver, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        onDispose {
            context.unregisterReceiver(batteryReceiver)
        }
    }
    
    val launcher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let {
            statusText = "Loading..."
            coroutineScope.launch(Dispatchers.IO) {
                try {
                    val cacheFile = java.io.File(context.cacheDir, "game_tmp_${System.currentTimeMillis()}.bin")
                    context.contentResolver.openInputStream(it)?.use { input ->
                        cacheFile.outputStream().use { output -> input.copyTo(output) }
                    }
                    val path = cacheFile.absolutePath
                    // Simulate loading since we mocked native
                    kotlinx.coroutines.delay(500)
                    gameViewModel.insert(GameEntity(
                        id = System.currentTimeMillis().toString(),
                        name = path.substringAfterLast("/").substringBeforeLast("."),
                        path = path
                    ))
                    statusText = "Game loaded"
                } catch (e: Exception) {
                    statusText = "Error: ${e.message}"
                }
            }
        }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                brush = Brush.verticalGradient(
                    colors = listOf(Color(0xFF1A1A1A), Color(0xFF000000))
                )
            )
    ) {
        Column(modifier = Modifier.fillMaxSize()) {
            // Top Bar
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 48.dp, vertical = 32.dp)
                    .windowInsetsPadding(WindowInsets.statusBars),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(onClick = onOpenDrawer) {
                    Icon(imageVector = Icons.Default.Menu, contentDescription = "Menu", tint = Color.White)
                }
                Spacer(modifier = Modifier.width(16.dp))
                Text(
                    text = "Games",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = Color.White
                )
                Spacer(modifier = Modifier.width(24.dp))
                Text(
                    text = "Media",
                    fontSize = 20.sp,
                    color = Color.Gray
                )
                Spacer(modifier = Modifier.weight(1f))
                IconButton(onClick = { }) {
                    Icon(imageVector = Icons.Default.Search, contentDescription = "Search", tint = Color.White)
                }
                IconButton(onClick = onSettingsClick) {
                    Icon(imageVector = Icons.Default.Settings, contentDescription = "Settings", tint = Color.White)
                }
                Spacer(modifier = Modifier.width(16.dp))
                Box(
                    modifier = Modifier
                        .size(36.dp)
                        .clip(CircleShape)
                        .background(Color.DarkGray)
                        .clickable { onSettingsClick() },
                    contentAlignment = Alignment.Center
                ) {
                    Icon(imageVector = Icons.Default.Person, contentDescription = "Profile", tint = Color.White, modifier = Modifier.size(24.dp))
                }
                Spacer(modifier = Modifier.width(16.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    if (batteryLevel >= 0) {
                        BatteryIcon(level = batteryLevel, isCharging = batteryIsCharging)
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(
                            text = "$batteryLevel%",
                            fontSize = 14.sp,
                            color = Color.White,
                            fontWeight = FontWeight.Medium
                        )
                    } else {
                        Text(
                            text = "...",
                            fontSize = 14.sp,
                            color = Color.White
                        )
                    }
                }
                Spacer(modifier = Modifier.width(16.dp))
                SystemTimeDisplay()
            }
            
            // Game Row
            LazyRow(
                contentPadding = PaddingValues(horizontal = 48.dp),
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                item {
                    val isSelected = selectedIndex == 0
                    val size by animateDpAsState(targetValue = if (isSelected) 84.dp else 72.dp)
                    Box(
                        modifier = Modifier
                            .size(size)
                            .clip(RoundedCornerShape(16.dp))
                            .background(if (isSelected) Color.White.copy(alpha = 0.2f) else Color(0xFF2A2A2A))
                            .onFocusChanged { if (it.isFocused) selectedIndex = 0 }
                            .clickable { if (isSelected) onLibraryClick() else selectedIndex = 0 },
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.Add,
                            contentDescription = "App Library",
                            tint = Color.White,
                            modifier = Modifier.size(32.dp)
                        )
                    }
                }
                itemsIndexed(games) { index, game ->
                    val actualIndex = index + 1
                    val isSelected = selectedIndex == actualIndex
                    val size by animateDpAsState(targetValue = if (isSelected) 84.dp else 72.dp)
                    Box(
                        modifier = Modifier
                            .size(size)
                            .clip(RoundedCornerShape(16.dp))
                            .background(if (isSelected) Color.White.copy(alpha = 0.2f) else Color(0xFF2A2A2A))
                            .onFocusChanged { if (it.isFocused) selectedIndex = actualIndex }
                            .clickable { 
                                if (isSelected) onGameSelected(game.path)
                                else selectedIndex = actualIndex 
                            },
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = game.name.take(2).uppercase(),
                            color = Color.White,
                            fontWeight = FontWeight.Bold,
                            fontSize = 24.sp
                        )
                    }
                }
            }
            
            Spacer(modifier = Modifier.height(48.dp))
            
            // Selected Item Details
            Box(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .padding(horizontal = 48.dp)
            ) {
                if (selectedIndex == 0) {
                    Column {
                        Text("App Library", fontSize = 32.sp, fontWeight = FontWeight.Bold, color = Color.White)
                        Spacer(modifier = Modifier.height(8.dp))
                        Text("Install or launch new applications offline.", color = Color.Gray, fontSize = 16.sp)
                        Spacer(modifier = Modifier.height(24.dp))
                        Button(
                            onClick = { launcher.launch("*/*") },
                            colors = ButtonDefaults.buttonColors(containerColor = Color.White, contentColor = Color.Black),
                            shape = RoundedCornerShape(24.dp),
                            contentPadding = PaddingValues(horizontal = 32.dp, vertical = 16.dp)
                        ) {
                            Text("Add Game", fontWeight = FontWeight.Bold, fontSize = 16.sp)
                        }
                    }
                } else {
                    val game = games.getOrNull(selectedIndex - 1)
                    if (game != null) {
                        Column {
                            Text(game.name, fontSize = 32.sp, fontWeight = FontWeight.Bold, color = Color.White)
                            Spacer(modifier = Modifier.height(8.dp))
                            Text("Local Application • Version ${game.version}", color = Color.Gray, fontSize = 16.sp)
                            Spacer(modifier = Modifier.height(4.dp))
                            Text("Last Played: ${game.lastPlayed} • Play Time: ${game.playTime}", color = Color.Gray, fontSize = 14.sp)
                            Spacer(modifier = Modifier.height(24.dp))
                            Button(
                                onClick = { onGameSelected(game.path) },
                                colors = ButtonDefaults.buttonColors(containerColor = Color.White, contentColor = Color.Black),
                                shape = RoundedCornerShape(24.dp),
                                contentPadding = PaddingValues(horizontal = 32.dp, vertical = 16.dp)
                            ) {
                                Text("Play", fontWeight = FontWeight.Bold, fontSize = 16.sp)
                            }
                        }
                    }
                }
            }
            
            // Bottom Status
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(24.dp)
                    .windowInsetsPadding(WindowInsets.navigationBars)
            ) {
                Text(
                    text = statusText,
                    color = Color.DarkGray,
                    fontSize = 13.sp
                )
            }
        }
    }
}

@Composable
fun EmulationScreen(path: String) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = "Emulating: ${path.substringAfterLast("/")}",
            color = Color.White,
            fontSize = 20.sp
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun PX5GameCard(
    game: GameEntity,
    onClick: () -> Unit,
    onFavoriteClick: () -> Unit = {},
    onHideClick: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    var showMenu by remember { mutableStateOf(false) }
    val interactionSource = remember { MutableInteractionSource() }
    val isFocused by interactionSource.collectIsFocusedAsState()
    val isHovered by interactionSource.collectIsHoveredAsState()
    val isPressed by interactionSource.collectIsPressedAsState()
    val isActive = isFocused || isHovered || isPressed

    val scale by animateFloatAsState(targetValue = if (isActive) 1.05f else 1.0f, animationSpec = tween(300))
    
    Box(
        modifier = modifier
            .aspectRatio(180f / 240f)
            .scale(scale)
            .clip(RoundedCornerShape(12.dp))
            .background(Color(0xFF2A2A2A))
            .border(
                width = if (isActive) 2.dp else 0.dp,
                color = if (isActive) Color.White.copy(alpha = 0.5f) else Color.Transparent,
                shape = RoundedCornerShape(12.dp)
            )
            .combinedClickable(
                interactionSource = interactionSource,
                indication = null,
                onClick = onClick,
                onLongClick = { showMenu = true }
            ),
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.padding(16.dp)) {
            Text(game.name.take(2).uppercase(), color = Color.White, fontSize = 48.sp, fontWeight = FontWeight.Bold)
            Spacer(modifier = Modifier.height(16.dp))
            Text(game.name, color = Color.LightGray, fontSize = 14.sp, textAlign = TextAlign.Center)
        }
        
        DropdownMenu(
            expanded = showMenu,
            onDismissRequest = { showMenu = false }
        ) {
            DropdownMenuItem(
                text = { Text("View Details") },
                onClick = {
                    showMenu = false
                    onClick()
                }
            )
            DropdownMenuItem(
                text = { Text(if (game.isFavorite) "Unfavorite" else "Favorite") },
                onClick = {
                    showMenu = false
                    onFavoriteClick()
                    Toast.makeText(context, if (game.isFavorite) "Removed from favorites" else "Added to favorites", Toast.LENGTH_SHORT).show()
                }
            )
            DropdownMenuItem(
                text = { Text("Hide Game") },
                onClick = {
                    showMenu = false
                    onHideClick()
                    Toast.makeText(context, "Game hidden", Toast.LENGTH_SHORT).show()
                }
            )
        }
    }
}

@Composable
fun BatteryIcon(level: Int, isCharging: Boolean, modifier: Modifier = Modifier) {
    Canvas(modifier = modifier.size(24.dp, 12.dp)) {
        val strokeWidth = 1.dp.toPx()
        val corner = 2.dp.toPx()
        val padding = 2.dp.toPx()
        
        // Draw main battery body
        drawRoundRect(
            color = Color.White,
            style = Stroke(width = strokeWidth),
            cornerRadius = CornerRadius(corner, corner),
            size = Size(size.width - 2.dp.toPx(), size.height)
        )
        
        // Draw positive terminal
        drawRect(
            color = Color.White,
            topLeft = Offset(size.width - 2.dp.toPx(), size.height * 0.25f),
            size = Size(2.dp.toPx(), size.height * 0.5f)
        )
        
        // Draw fill level
        if (level > 0) {
            val fillWidth = (size.width - 2.dp.toPx() - padding * 2) * (level / 100f)
            drawRoundRect(
                color = Color.White,
                topLeft = Offset(padding, padding),
                size = Size(fillWidth, size.height - padding * 2),
                cornerRadius = CornerRadius(1.dp.toPx(), 1.dp.toPx())
            )
        }
        
        // Draw charging bolt
        if (isCharging) {
            val path = Path().apply {
                val cx = (size.width - 2.dp.toPx()) / 2
                val cy = size.height / 2
                moveTo(cx + 1.dp.toPx(), cy - 3.dp.toPx())
                lineTo(cx - 1.dp.toPx(), cy + 0.dp.toPx())
                lineTo(cx + 1.dp.toPx(), cy + 0.dp.toPx())
                lineTo(cx - 1.dp.toPx(), cy + 3.dp.toPx())
                lineTo(cx + 2.dp.toPx(), cy)
                lineTo(cx, cy)
                close()
            }
            drawPath(
                path = path,
                color = Color.Black
            )
        }
    }
}

@Composable
fun SystemTimeDisplay() {
    var currentTime by remember { mutableStateOf("") }
    
    LaunchedEffect(Unit) {
        while (true) {
            val calendar = java.util.Calendar.getInstance()
            val hour = calendar.get(java.util.Calendar.HOUR_OF_DAY)
            val minute = calendar.get(java.util.Calendar.MINUTE)
            currentTime = String.format(java.util.Locale.getDefault(), "%02d:%02d", hour, minute)
            kotlinx.coroutines.delay(1000)
        }
    }
    
    Text(
        text = currentTime,
        fontSize = 16.sp,
        color = Color.White
    )
}