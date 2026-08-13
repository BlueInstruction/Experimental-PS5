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

        // Initialize FEXCore JNI Core & Load ThunksDB / FEX Config
        // Wrapped in try/catch so a native init failure doesn't kill the app
        // before we can log it.
        writeDiagnosticLog("MainActivity.onCreate: loading libpx5.so via FexCoreWrapper")
        try {
            fexCoreWrapper = FexCoreWrapper()
            writeDiagnosticLog("MainActivity.onCreate: FexCoreWrapper() constructed — libpx5.so loaded")

            val initSuccess = fexCoreWrapper!!.initializeFexCore()
            fexCoreStatus = if (initSuccess) "Ready (${fexCoreWrapper!!.stringFromJNI()})" else "Initialized (JNI)"
            writeDiagnosticLog("MainActivity.onCreate: initializeFexCore() returned $initSuccess")

            // Load ThunksDB and FEX Config from assets
            try {
                val thunksStr = assets.open("ThunksDB.json").bufferedReader().use { it.readText() }
                fexCoreWrapper!!.nativeLoadThunksConfig(thunksStr)

                val fexConfigStr = assets.open("fex_config.json").bufferedReader().use { it.readText() }
                fexCoreWrapper!!.nativeLoadFexConfig(fexConfigStr)
                writeDiagnosticLog("MainActivity.onCreate: ThunksDB & fex_config loaded")
            } catch (assetErr: Exception) {
                writeDiagnosticLog("MainActivity.onCreate: FEX assets load FAILED: ${assetErr.message}")
            }
        } catch (e: Throwable) {
            fexCoreStatus = "Error: ${e.message}"
            writeDiagnosticLog("MainActivity.onCreate: FEXCore init FAILED: ${e.javaClass.name}: ${e.message}\n${e.stackTraceToString()}")
        }

        // If FEXCore failed to init, we still need a non-null FexCoreWrapper
        // for AppNavigation (which calls nativeInitAdrenotools on it).
        // Re-throw as a RuntimeException so the global handler captures it
        // AND writes to the crash log. Without this, AppNavigation would
        // get a null FexCoreWrapper and crash with a confusing NPE.
        if (fexCoreWrapper == null) {
            val err = RuntimeException("FexCoreWrapper is null — libpx5.so failed to load or initializeFexCore threw. Check px5_diagnostic.log for the step-by-step trace.")
            writeDiagnosticLog("MainActivity.onCreate: fexCoreWrapper is null, throwing to global handler")
            throw err
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
