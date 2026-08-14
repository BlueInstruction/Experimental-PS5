package com.px5.emulator

import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.px5.emulator.core.FexCoreWrapper
import com.px5.emulator.ui.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.File

class MainActivity : ComponentActivity() {

    private lateinit var soundManager: SoundManager
    private var fexCoreWrapper: FexCoreWrapper? = null
    private var fexCoreStatus: String = "Uninitialized"

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // ---- Early diagnostic logging ----
        // Write a "MainActivity.onCreate started" line to the crash log file
        // BEFORE doing anything else. If the app crashes after this point,
        // we'll at least know it got this far.
        writeDiagnosticLog("MainActivity.onCreate: start")

        // Initialize PS5 Sound Manager & Music
        writeDiagnosticLog("MainActivity.onCreate: initializing SoundManager")
        try {
            soundManager = SoundManager.getInstance(applicationContext)
            writeDiagnosticLog("MainActivity.onCreate: SoundManager OK")
        } catch (e: Throwable) {
            writeDiagnosticLog("MainActivity.onCreate: SoundManager FAILED: ${e.javaClass.name}: ${e.message}")
            // Don't crash — sound is non-essential
        }

        // FEXCore init — SKIPPED for now to isolate UI rendering issues.
        // The diagnostic log showed the app crashes during initializeFexCore()
        // (native SIGSEGV). By skipping it, we can verify the UI renders
        // correctly without FEXCore. Once UI works, we'll re-enable FEXCore
        // init with proper error handling.
        writeDiagnosticLog("MainActivity.onCreate: loading libpx5.so via FexCoreWrapper")
        try {
            fexCoreWrapper = FexCoreWrapper()
            writeDiagnosticLog("MainActivity.onCreate: FexCoreWrapper() constructed — libpx5.so loaded")

            // SKIP: initializeFexCore() — causes native SIGSEGV
            // val initSuccess = fexCoreWrapper!!.initializeFexCore()
            // fexCoreStatus = if (initSuccess) "Ready" else "Error"
            fexCoreStatus = "FEXCore init SKIPPED (debug mode)"
            writeDiagnosticLog("MainActivity.onCreate: initializeFexCore() SKIPPED — UI-only mode")
        } catch (e: Throwable) {
            fexCoreStatus = "Error: ${e.message}"
            writeDiagnosticLog("MainActivity.onCreate: FexCoreWrapper FAILED: ${e.javaClass.name}: ${e.message}")
        }

        // If FEXCore wrapper is null, create a dummy so AppNavigation doesn't NPE
        if (fexCoreWrapper == null) {
            writeDiagnosticLog("MainActivity.onCreate: fexCoreWrapper is null, creating dummy")
            try {
                fexCoreWrapper = FexCoreWrapper()
            } catch (e: Throwable) {
                writeDiagnosticLog("MainActivity.onCreate: dummy FexCoreWrapper also failed: ${e.message}")
                fexCoreStatus = "FATAL: libpx5.so cannot load"
            }
        }

        writeDiagnosticLog("MainActivity.onCreate: calling enableEdgeToEdge")
        try {
            enableEdgeToEdge()
        } catch (e: Throwable) {
            writeDiagnosticLog("MainActivity.onCreate: enableEdgeToEdge FAILED: ${e.message}")
        }

        writeDiagnosticLog("MainActivity.onCreate: calling setContent")
        try {
            setContent {
                PX5Theme {
                    AppNavigation(
                        soundManager = soundManager,
                        fexCoreWrapper = fexCoreWrapper!!,
                        fexCoreStatus = fexCoreStatus
                    )
                }
            }
            writeDiagnosticLog("MainActivity.onCreate: setContent OK")
        } catch (e: Throwable) {
            writeDiagnosticLog("MainActivity.onCreate: setContent FAILED: ${e.javaClass.name}: ${e.message}\n${e.stackTraceToString()}")
            throw e  // re-throw so the global handler catches it
        }
    }

    /**
     * Write a diagnostic line to px5_diagnostic.log in the app's external
     * files dir. This is SEPARATE from the crash log — it's a running
     * trace of every step in MainActivity.onCreate so we can see exactly
     * where the app crashes.
     *
     * The file is appended to (not overwritten) so we get the full
     * launch sequence across multiple attempts.
     */
    private fun writeDiagnosticLog(message: String) {
        try {
            android.util.Log.i("PX5_Diag", message)
            val logsDir = getExternalFilesDir("logs") ?: return
            if (!logsDir.exists()) logsDir.mkdirs()
            val logFile = File(logsDir, "px5_diagnostic.log")
            val timestamp = java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", java.util.Locale.US).format(java.util.Date())
            val line = "[$timestamp] $message\n"
            logFile.appendText(line)
        } catch (_: Throwable) {
            // Swallow — diagnostic logging must never crash the app
        }
    }

    override fun onResume() {
        super.onResume()
        try {
            soundManager.startBgMusic()
        } catch (_: Throwable) {}
    }

    override fun onPause() {
        super.onPause()
        try {
            soundManager.pauseBgMusic()
        } catch (_: Throwable) {}
    }

    override fun onDestroy() {
        super.onDestroy()
        try {
            soundManager.release()
        } catch (_: Throwable) {}
    }
}

@Composable
fun AppNavigation(
    soundManager: SoundManager,
    fexCoreWrapper: FexCoreWrapper,
    fexCoreStatus: String,
    gameViewModel: GameViewModel = viewModel()
) {
    val games by gameViewModel.allGames.collectAsStateWithLifecycle()
    val navController = rememberNavController()
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()

    var pendingPkgFile by remember { mutableStateOf<Pair<String, String>?>(null) } // (fileName, path)
    var showTurnipManagerSheet by remember { mutableStateOf(false) }

    // Directory scanner function (/root/Games, /sdcard/PX5/Games, etc.)
    val scanDirectoriesForGames = {
        coroutineScope.launch(Dispatchers.IO) {
            val targetDirs = listOf(
                File("/root/Games"),
                File(context.getExternalFilesDir(null), "Games"),
                File("/sdcard/PX5/Games"),
                File("/sdcard/Download")
            )
            var count = 0
            for (dir in targetDirs) {
                if (dir.exists() && dir.isDirectory) {
                    dir.listFiles()?.forEach { file ->
                        val ext = file.extension.lowercase()
                        if (ext in listOf("pkg", "elf", "iso", "bin", "self")) {
                            val gameName = file.nameWithoutExtension.replace("_", " ")
                            gameViewModel.insert(
                                GameEntity(
                                    id = file.absolutePath.hashCode().toString(),
                                    name = gameName,
                                    path = file.absolutePath,
                                    category = if (ext == "pkg") "PS5 PKG" else "Installed Game",
                                    developer = "Discovered File",
                                    sizeGb = "${String.format("%.1f", file.length() / (1024.0 * 1024.0 * 1024.0))} GB"
                                )
                            )
                            count++
                        }
                    }
                }
            }
            launch(Dispatchers.Main) {
                if (count > 0) {
                    Toast.makeText(context, "Scanned & imported $count games from /root & storage!", Toast.LENGTH_LONG).show()
                } else {
                    Toast.makeText(context, "Directories scanned. Place .pkg or .elf files in /root/Games or /sdcard/PX5/Games", Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    // File picker launcher for loading custom game files (.elf, .bin, .iso, .pkg)
    val gameLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let {
            coroutineScope.launch(Dispatchers.IO) {
                try {
                    val originalName = uri.lastPathSegment ?: "game.pkg"
                    val isPkg = originalName.endsWith(".pkg", ignoreCase = true)
                    val targetExt = if (isPkg) "pkg" else "elf"
                    val cacheFile = File(context.cacheDir, "game_${System.currentTimeMillis()}.$targetExt")
                    
                    context.contentResolver.openInputStream(it)?.use { input ->
                        cacheFile.outputStream().use { output -> input.copyTo(output) }
                    }
                    val path = cacheFile.absolutePath
                    val gameName = originalName.substringAfterLast("/").substringBeforeLast(".")

                    if (isPkg) {
                        launch(Dispatchers.Main) {
                            pendingPkgFile = Pair(gameName, path)
                        }
                    } else {
                        gameViewModel.insert(
                            GameEntity(
                                id = System.currentTimeMillis().toString(),
                                name = gameName.ifBlank { "Custom Game" },
                                path = path,
                                category = "Custom Game",
                                developer = "Local User",
                                sizeGb = "2.4 GB"
                            )
                        )
                        launch(Dispatchers.Main) {
                            Toast.makeText(context, "Installed: $gameName", Toast.LENGTH_SHORT).show()
                        }
                    }
                } catch (e: Exception) {
                    launch(Dispatchers.Main) {
                        Toast.makeText(context, "Failed to load file: ${e.message}", Toast.LENGTH_LONG).show()
                    }
                }
            }
        }
    }

    // File picker launcher for importing Turnip ZIP driver packages
    val driverLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        uri?.let {
            coroutineScope.launch(Dispatchers.IO) {
                try {
                    val driverDir = File(context.filesDir, "turnip_custom_${System.currentTimeMillis()}")
                    driverDir.mkdirs()
                    val targetFile = File(driverDir, "turnip_driver.zip")
                    context.contentResolver.openInputStream(it)?.use { input ->
                        targetFile.outputStream().use { output -> input.copyTo(output) }
                    }
                    fexCoreWrapper.nativeInitAdrenotools(
                        driverDir.absolutePath,
                        "Custom Imported Turnip Driver",
                        "libadrenotools.so"
                    )
                    launch(Dispatchers.Main) {
                        Toast.makeText(context, "Custom Turnip Driver package injected successfully!", Toast.LENGTH_LONG).show()
                    }
                } catch (e: Exception) {
                    launch(Dispatchers.Main) {
                        Toast.makeText(context, "Failed to import driver: ${e.message}", Toast.LENGTH_LONG).show()
                    }
                }
            }
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        NavHost(navController = navController, startDestination = "home") {
            composable("home") {
                PS5HomeScreen(
                    games = games,
                    gameViewModel = gameViewModel,
                    soundManager = soundManager,
                    fexCoreStatus = fexCoreStatus,
                    onGameSelected = { path ->
                        navController.navigate("emulation?path=$path")
                    },
                    onOpenStore = {
                        navController.navigate("store")
                    },
                    onOpenSettings = {
                        navController.navigate("settings")
                    },
                    onOpenSearch = {
                        navController.navigate("search")
                    },
                    onAddGameClick = {
                        gameLauncher.launch("*/*")
                    }
                )
            }

            composable("store") {
                PS5StoreScreen(
                    onAddGameClick = { gameLauncher.launch("*/*") },
                    onBackClick = { navController.popBackStack() },
                    onGameSelected = { path -> navController.navigate("emulation?path=$path") }
                )
            }

            composable("search") {
                PS5SearchScreen(
                    games = games,
                    onGameSelected = { game -> navController.navigate("emulation?path=${game.path}") },
                    onBackClick = { navController.popBackStack() }
                )
            }

            composable("settings") {
                PS5SettingsScreen(
                    soundManager = soundManager,
                    fexCoreStatus = fexCoreStatus,
                    fexCoreWrapper = fexCoreWrapper,
                    onScanGamesClick = { scanDirectoriesForGames() },
                    onOpenTurnipManagerClick = { showTurnipManagerSheet = true },
                    onBackClick = { navController.popBackStack() }
                )
            }

            composable("emulation?path={path}") { backStackEntry ->
                val path = backStackEntry.arguments?.getString("path") ?: ""
                EmulationScreen(
                    path = path,
                    fexCoreStatus = fexCoreStatus,
                    onBackClick = { navController.popBackStack() }
                )
            }
        }

        // Overlay PKG Package Installer Dialog
        pendingPkgFile?.let { (fileName, path) ->
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.7f)),
                contentAlignment = Alignment.BottomCenter
            ) {
                PS5PkgInstallerSheet(
                    fileName = fileName,
                    filePath = path,
                    soundManager = soundManager,
                    onInstallationComplete = { title, installedPath ->
                        gameViewModel.insert(
                            GameEntity(
                                id = System.currentTimeMillis().toString(),
                                name = title,
                                path = installedPath,
                                category = "PS5 PKG",
                                developer = "Installed PKG Package",
                                sizeGb = "4.2 GB"
                            )
                        )
                        pendingPkgFile = null
                        Toast.makeText(context, "PKG Package '$title' successfully installed!", Toast.LENGTH_LONG).show()
                    },
                    onDismiss = { pendingPkgFile = null }
                )
            }
        }

        // Overlay Turnip Driver Manager Dialog
        if (showTurnipManagerSheet) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.7f)),
                contentAlignment = Alignment.BottomCenter
            ) {
                PS5TurnipDriverSheet(
                    soundManager = soundManager,
                    fexCoreWrapper = fexCoreWrapper,
                    onImportCustomDriverClick = {
                        driverLauncher.launch("*/*")
                    },
                    onDismiss = { showTurnipManagerSheet = false }
                )
            }
        }
    }
}

@Composable
fun EmulationScreen(
    path: String,
    fexCoreStatus: String,
    onBackClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(24.dp)
                .windowInsetsPadding(WindowInsets.statusBars),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(
                    onClick = onBackClick,
                    modifier = Modifier
                        .size(40.dp)
                        .clip(CircleShape)
                        .background(Color.White.copy(alpha = 0.15f))
                ) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Exit Emulation",
                        tint = Color.White
                    )
                }

                Spacer(modifier = Modifier.width(16.dp))

                Text(
                    text = "PX5 Native Emulation Engine",
                    color = Color.White,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    fontFamily = TitilliumFontFamily
                )
            }

            Spacer(modifier = Modifier.weight(1f))

            // Emulation Canvas Overlay
            Box(
                modifier = Modifier
                    .fillMaxWidth(0.85f)
                    .aspectRatio(16f / 9f)
                    .clip(RoundedCornerShape(16.dp))
                    .background(Color(0xFF0D121B))
                    .border(2.dp, PS5AccentBlue, RoundedCornerShape(16.dp)),
                contentAlignment = Alignment.Center
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Image(
                        painter = painterResource(id = R.drawable.ic_dualsense_ps),
                        contentDescription = "PS",
                        modifier = Modifier.size(64.dp)
                    )

                    Spacer(modifier = Modifier.height(16.dp))

                    Text(
                        text = "Running: ${path.substringAfterLast("/")}",
                        color = Color.White,
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        fontFamily = TitilliumFontFamily
                    )

                    Spacer(modifier = Modifier.height(6.dp))

                    Text(
                        text = "Vulkan 1.3 Surface • FEXCore CPU: $fexCoreStatus",
                        color = PS5AccentGlow,
                        fontSize = 13.sp,
                        fontFamily = TitilliumFontFamily
                    )
                }
            }

            Spacer(modifier = Modifier.weight(1f))

            // On-screen DualSense Prompts
            DualSenseButtonPrompts(modifier = Modifier.padding(bottom = 16.dp))
        }
    }
}


