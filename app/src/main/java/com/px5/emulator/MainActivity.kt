package com.px5.emulator

import android.annotation.SuppressLint
import android.net.Uri
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.px5.emulator.core.FexCoreWrapper
import com.px5.emulator.core.PX5EventLog
import com.px5.emulator.core.Px5Settings
import com.px5.emulator.ui.EmuScreen
import com.px5.emulator.ui.PS5HomeScreen
import com.px5.emulator.ui.PS5LogsScreen
import com.px5.emulator.ui.PS5SettingsScreen
import com.px5.emulator.ui.PS5TurnipDriverSheet
import com.px5.emulator.ui.PX5Theme
import com.px5.emulator.ui.TitilliumFontFamily
import com.px5.emulator.ui.px5Colors
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import java.io.File

/**
 * Main activity for the PX5 emulator, handling initialization, navigation, and input.
 */
class MainActivity : ComponentActivity() {

    private lateinit var soundManager: SoundManager
    private var fexCoreWrapper: FexCoreWrapper? = null
    private var fexCoreStatus: String = "Uninitialized"

    /**
     * Dispatches physical gamepad key events before Compose (handheld consoles + BT pads).
     * Only active while EmuScreen is on screen.
     * @param event The key event
     * @return true if event was consumed, false otherwise
     */
    // RestrictedApi fires only because androidx.core is a different library
    // group than PSX5; overriding dispatchKeyEvent on an Activity is its
    // intended use.
    @SuppressLint("RestrictedApi")
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (PhysicalControllerBridge.enabled &&
            PhysicalControllerBridge.handleKey(event)) return true
        return super.dispatchKeyEvent(event)
    }

    /**
     * Dispatches physical gamepad motion events (analog sticks, triggers).
     * @param event The motion event
     * @return true if event was consumed, false otherwise
     */
    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (PhysicalControllerBridge.enabled &&
            PhysicalControllerBridge.handleMotion(event)) return true
        return super.dispatchGenericMotionEvent(event)
    }

    /**
     * Initializes the activity: logging, build identity, sound, settings, FEXCore, UI.
     * @param savedInstanceState Previous state if any
     */
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // ---- Early diagnostic logging ----
        // The shared event stream must exist before anything else emits.
        PX5EventLog.init(applicationContext)
        logEvent("MainActivity", "onCreate", "started")

        // ---- Build identity (honest, from the running process) ----
        // Every pasted log now self-identifies: which APK version, which
        // git commit was compiled in, which FEXCore pin the build stamped,
        // and whether the adrenotools runtime hook libraries are actually
        // packaged — the exact discriminator that was missing between the
        // 2026-08-28 23:38 (restored_0) and 23:45 (restored_2) launches.
        try {
            val hookDir = applicationInfo.nativeLibraryDir
            val haveImpl = File(hookDir, "libhook_impl.so").isFile
            val haveMain = File(hookDir, "libmain_hook.so").isFile
            logState("build", "v${BuildConfig.VERSION_NAME} " +
                    "vc${BuildConfig.VERSION_CODE} sha=${BuildConfig.GIT_SHA}")
            logEvent("build", "identity",
                    "fexcore=${BuildConfig.FEXCORE_PIN.replace(" ", "")} " +
                    "api=${android.os.Build.VERSION.SDK_INT} " +
                    "abi=${android.os.Build.SUPPORTED_ABIS.firstOrNull()} " +
                    "hooks=${if (haveImpl && haveMain) "packaged" else "impl=$haveImpl,main=$haveMain"}")
        } catch (e: Throwable) {
            logException("build.identity", e)
        }

        // --- SoundManager ---
        logState("sound", "initializing")
        try {
            soundManager = SoundManager.getInstance(applicationContext)
            logState("sound", "ready")
        } catch (e: Throwable) {
            logState("sound", "error")
            logException("SoundManager.init", e)
        }

        // --- Settings store (native-coupled) ------------------------------
        // Must run BEFORE initializeFexCore: FEXCore config overrides read
        // the layered store at context creation, so the preset has to be in
        // place first. The old order (settings after engine init) silently
        // discarded every preset on cold start.
        Px5Settings.init(applicationContext)
        // Honor the persisted shell orientation (system / landscape / portrait).
        Px5Settings.applyOrientation(this)

        // --- FEXCore ---
        // initializeFexCore() runs the full engine bring-up. On the one
        // device class that faulted here (2026-08-28 logs), the crash was
        // inside guest execution — NOT InitCore — and is now contained by
        // the fork-isolated test harness (see nativeRunCpuConformanceTest).
        logState("fex", "loading_library")
        try {
            fexCoreWrapper = FexCoreWrapper()
            logState("fex", "library_loaded")
            logEvent("FEXCore", "library_loaded", "libpx5.so")

            // FEXCore preset + diagnostics gates go in BEFORE the context
            // exists. Failures are logged by the native side and do not
            // block startup (honest: a rejected key stays rejected).
            try {
                Px5Settings.engineOverrides.value.forEach { (k, v) ->
                    fexCoreWrapper?.nativeApplyEngineConfigOverride(k, v)
                }
                val lvl = Px5Settings.logLevel.value
                if (lvl >= 0) fexCoreWrapper?.nativeSetLogLevel(lvl)
                fexCoreWrapper?.nativeSetPresentMode(Px5Settings.presentMode.value)
            } catch (e: Throwable) {
                logException("FEXCore.applyPreset", e)
            }

            logState("fex", "initializing_runtime")
            logEvent("FEXCore", "initializeRuntime", "called")
            try {
                val initResult = fexCoreWrapper!!.initializeFexCore()
                logEvent("FEXCore", "initializeRuntime", "returned", initResult.toString())
                if (initResult) {
                    logState("fex", "runtime_ready")
                    fexCoreStatus = "Ready"
                } else {
                    logState("fex", "runtime_error")
                    fexCoreStatus = "Error"
                }
            } catch (e: Throwable) {
                logState("fex", "runtime_crashed")
                logException("FEXCore.initializeRuntime", e)
                fexCoreStatus = "Error: ${e.message}"
            }
        } catch (e: Throwable) {
            logState("fex", "library_load_failed")
            logException("FexCoreWrapper.constructor", e)
            fexCoreStatus = "Error: ${e.message}"
        }

        if (fexCoreWrapper == null) {
            logState("fex", "creating_fallback_wrapper")
            try {
                fexCoreWrapper = FexCoreWrapper()
                logState("fex", "fallback_wrapper_created")
            } catch (e: Throwable) {
                logState("fex", "fallback_wrapper_failed")
                logException("FexCoreWrapper.fallback", e)
                fexCoreStatus = "FATAL: libpx5.so cannot load"
            }
        }

        // --- Runtime context wiring (crash handler + driver dirs) ---------
        // Uses the SAME wrapper instance created above. The previous code
        // built a throwaway FexCoreWrapper() here, which double-installed
        // the native crash handler through JNI_OnLoad side effects.
        fexCoreWrapper?.let { wrapper ->
            logState("runtime", "wiring_context")
            try {
                val logsDir = getExternalFilesDir("logs")?.absolutePath
                    ?: filesDir.resolve("logs").absolutePath
                java.io.File(logsDir).mkdirs()
                PhysicalControllerBridge.wrapper = wrapper
                // Build identity goes into the NATIVE streams as well — the
                // 2026-08-29 paste proved the engine-log side had no idea
                // which APK produced it.
                val hookDir = applicationInfo.nativeLibraryDir
                val haveImplNative = File(hookDir, "libhook_impl.so").isFile
                val haveMainNative = File(hookDir, "libmain_hook.so").isFile
                val buildIdentity =
                    "v${BuildConfig.VERSION_NAME} vc${BuildConfig.VERSION_CODE} " +
                    "sha=${BuildConfig.GIT_SHA} " +
                    "fexcore=${BuildConfig.FEXCORE_PIN.replace(" ", "")} " +
                    "api=${android.os.Build.VERSION.SDK_INT} " +
                    "abi=${android.os.Build.SUPPORTED_ABIS.firstOrNull()} " +
                    "hooks=${if (haveImplNative && haveMainNative) "packaged" else "impl=$haveImplNative,main=$haveMainNative"}"
                wrapper.nativeInitRuntimeContext(
                    logsDir,
                    applicationInfo.nativeLibraryDir,
                    cacheDir.absolutePath,
                    filesDir.absolutePath,
                    buildIdentity
                )
                logState("runtime", "context_ready")

                // Re-register persisted driver slots (cold start used to
                // forget imported Turnip drivers entirely).
                val liveSlots = DriverSlotStore.restore(applicationContext, wrapper,
                    onSlot = { label, soPath, soname ->
                        PX5EventLog.event("drivers", "slot_restored",
                            "label=$label soname=$soname path=$soPath")
                    })
                val savedMode = Px5Settings.driverMode.value
                if (liveSlots > 0 && savedMode in 1..liveSlots) {
                    wrapper.nativeSetDriverMode(savedMode)
                } else if (savedMode > liveSlots) {
                    Px5Settings.setDriverMode(0)
                    wrapper.nativeSetDriverMode(0)
                }
                logState("drivers", "restored_$liveSlots")
                Px5Settings.push(wrapper, applicationContext)
            } catch (e: Throwable) {
                logState("runtime", "context_failed")
                logException("RuntimeContext.wiring", e)
            }
        }

        logState("ui", "initializing")
        logEvent("UI", "enableEdgeToEdge", "called")
        try {
            enableEdgeToEdge()
            logEvent("UI", "enableEdgeToEdge", "completed")
        } catch (e: Throwable) {
            logEvent("UI", "enableEdgeToEdge", "failed")
            logException("UI.enableEdgeToEdge", e)
        }

        logEvent("UI", "setContent", "called")
        try {
            setContent {
                PX5Theme {
                    AppNavigation(
                        soundManager = soundManager,
                        fexCoreWrapper = fexCoreWrapper,
                        fexCoreStatus = fexCoreStatus
                    )
                }
            }
            logEvent("UI", "setContent", "completed")
            logState("ui", "ready")
        } catch (e: Throwable) {
            logEvent("UI", "setContent", "failed")
            logException("UI.setContent", e)
            logState("ui", "crashed")
            throw e
        }

        logState("app", "onCreate_completed")
    }

    // ========================================================================
    // 3-layer logging: Event / State / Diagnostic
    // ========================================================================

    /**
     * Logs an event with component, action, optional detail and result.
     * @param component Component name (e.g., "FEXCore", "UI")
     * @param action Action name (e.g., "initializeRuntime")
     * @param detail Optional detail string
     * @param result Optional result string
     */
    private fun logEvent(component: String, action: String, detail: String = "", result: String = "") {
        val msg = buildString {
            append("EVENT")
            append(" component=").append(component)
            append(" action=").append(action)
            if (detail.isNotEmpty()) append(" detail=").append(detail)
            if (result.isNotEmpty()) append(" result=").append(result)
        }
        writeLog(msg)
    }

    /**
     * Logs a state change for a component.
     * @param component Component name
     * @param state New state string
     */
    private fun logState(component: String, state: String) {
        writeLog("STATE component=$component state=$state")
    }

    /**
     * Logs an exception with source context and stack trace.
     * @param source Source context (e.g., "FexCoreWrapper.constructor")
     * @param e The exception to log
     */
    private fun logException(source: String, e: Throwable) {
        val sw = java.io.StringWriter()
        val pw = java.io.PrintWriter(sw)
        e.printStackTrace(pw)
        writeLog("DIAGNOSTIC source=$source exception=${e.javaClass.name} message=${e.message ?: "(none)"}\n${sw.toString().trim()}")
    }

    /**
     * Writes a log message to the app-wide event stream.
     * @param message Message to log
     */
    private fun writeLog(message: String) {
        // Delegates to the app-wide stream so every component (dialogs,
        // importers, coroutines) writes the same px5_diagnostic.log format.
        PX5EventLog.write(message)
    }

    /**
     * Called when activity resumes; starts background music.
     */
    override fun onResume() {
        super.onResume()
        logState("activity", "resumed")
        try {
            soundManager.startBgMusic()
        } catch (_: Throwable) {}
    }

    /**
     * Called when activity pauses; pauses background music.
     */
    override fun onPause() {
        super.onPause()
        logState("activity", "paused")
        try {
            soundManager.pauseBgMusic()
        } catch (_: Throwable) {}
    }

    /**
     * Called when activity is destroyed; releases sound resources.
     */
    override fun onDestroy() {
        super.onDestroy()
        logState("activity", "destroyed")
        try {
            soundManager.release()
        } catch (_: Throwable) {}
    }
}

/**
 * Main navigation composable for the app, handling home/settings/logs/emulation screens.
 * @param soundManager Sound manager instance
 * @param fexCoreWrapper FEXCore native wrapper
 * @param fexCoreStatus Current FEXCore status string
 * @param gameViewModel ViewModel for game database
 */
@Composable
fun AppNavigation(
    soundManager: SoundManager,
    fexCoreWrapper: FexCoreWrapper?,
    fexCoreStatus: String,
    gameViewModel: GameViewModel = viewModel()
) {
    val games by gameViewModel.allGames.collectAsStateWithLifecycle()
    val navController = rememberNavController()
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()

    var showTurnipManagerSheet by remember { mutableStateOf(false) }
    // v1.47: the manager sheet is an IN-ACTIVITY overlay — opening/closing
    // it never pauses the activity, so the settings screen's ON_RESUME
    // summary refresh never fires for sheet select/import/delete. The
    // 2026-09-05 23:30 device session caught the result: the sheet answered
    // "mode=1 slots=1 driverVerified=yes" while the settings line 13s later
    // still showed the composition-time "mode=0 slots=0 not-run". Every
    // driver-state change bumps this epoch; the settings Graphics tab
    // re-pulls the engine summary whenever it moves.
    var driverStateEpoch by remember { mutableStateOf(0) }
    var importStatus by remember { mutableStateOf<String?>(null) }
    var importBusy by remember { mutableStateOf(false) }

    fun launchImport(block: suspend () -> String) {
        if (importBusy) return
        importBusy = true
        importStatus = null
        coroutineScope.launch(Dispatchers.IO) {
            val msg = try {
                block()
            } catch (e: Exception) {
                "Import failed: ${e.message}"
            }
            kotlinx.coroutines.withContext(Dispatchers.Main) {
                importStatus = msg
                importBusy = false
            }
        }
    }

    // Pick a single file: .pkg / .iso / .elf / .self
    val importFileLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        uri?.let {
            launchImport {
                val report = GameImporter.importUri(
                    context.applicationContext, it,
                    add = { g -> gameViewModel.insert(g) },
                    onProgress = { p -> importStatus = p }
                )
                report.summary()
            }
        }
    }

    // Pick a whole folder: dumped games, exFAT dumps, sharpdroid/SharpEmu
    // folder trees — everything that follows the eboot.bin dump contract.
    val importFolderLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocumentTree()
    ) { uri: Uri? ->
        uri?.let {
            launchImport {
                val report = GameImporter.importUri(
                    context.applicationContext, it,
                    add = { g -> gameViewModel.insert(g) },
                    onProgress = { p -> importStatus = p }
                )
                report.summary()
            }
        }
    }

    fun scanStorage() {
        launchImport {
            val report = GameImporter.scanStorage(
                context.applicationContext,
                add = { g -> gameViewModel.insert(g) },
                onProgress = { p -> importStatus = p }
            )
            report.summary()
        }
    }

    // File picker launcher for importing GPU driver ZIP packages.
    //
    // Real pipeline: copy -> unzip (incl. one nested level) -> locate the
    // Vulkan driver library -> prove it is an AArch64 shared object ->
    // normalize its name to "libvulkan_adreno.so" (the exact soname
    // GpuDriverManager/adrenotools loads from the slot dir) -> register
    // slot in the native GpuDriverManager -> persist slot -> activate.
    //
    // Why the name scan is broad: driver packages in the wild name the
    // driver differently per build system — Mesa's Android build emits
    // "vulkan.turnip.so", some vendors ship "libvulkan.so.1", older
    // AdrenoTools packs use "libvulkan_adreno.so" and Skyline-era packs
    // "libvulkan.so". Matching the filename family, then validating the
    // ELF header, accepts all of them without ever trusting a lie.
    val driverLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        uri?.let {
            coroutineScope.launch(Dispatchers.IO) {
                var resultMsg: String
                try {
                    val slotRoot = File(context.filesDir, "driver_slots")
                    slotRoot.mkdirs()
                    val dir = File(slotRoot, "turnip_${System.currentTimeMillis()}")
                    dir.mkdirs()

                    val zipFile = File(dir, "package.zip")
                    context.contentResolver.openInputStream(it)?.use { input ->
                        zipFile.outputStream().use { output -> input.copyTo(output) }
                    } ?: throw IllegalStateException("cannot open selected file")

                    val seenNames = mutableListOf<String>()

                    fun extractZip(zf: File, into: File): Boolean {
                        var any = false
                        runCatching {
                            java.util.zip.ZipInputStream(zf.inputStream().buffered()).use { zis ->
                                var e = zis.nextEntry
                                while (e != null) {
                                    if (!e.isDirectory) {
                                        val outFile = File(into, e.name)
                                        if (!outFile.canonicalPath.startsWith(into.canonicalPath)) {
                                            throw SecurityException("zip path traversal blocked")
                                        }
                                        outFile.parentFile?.mkdirs()
                                        outFile.outputStream().use { out -> zis.copyTo(out) }
                                        seenNames += e.name
                                        if (e.name.lowercase().endsWith(".zip")) {
                                            // one nested level (some packs wrap the driver)
                                            extractZip(outFile, into)
                                            outFile.delete()
                                        }
                                        any = true
                                    }
                                    e = zis.nextEntry
                                }
                            }
                        }.onFailure { f -> android.util.Log.w("PX5", "zip scan: ${f.message}") }
                        return any
                    }
                    extractZip(zipFile, dir)
                    zipFile.delete()
                    PX5EventLog.event("driverImport", "zip_extracted",
                            "entries=${seenNames.size} dir=${dir.name}")

                    // --- meta.json (the ecosystem contract) -----------------
                    // Winlator/Eden/adrenotools driver packs carry a meta.json:
                    //   { schemaVersion, name, description, author, vendor,
                    //     driverVersion, minApi, libraryName }
                    // The .so file name is NOT stable across builds
                    // (vulkan.ad0863.so / vulkan.ad07xx.so /
                    // libvulkan_freedreno.so / ...) — libraryName is the
                    // authority. The filename scan below is only the
                    // fallback for packs without meta.json.
                    val metaFile = dir.walkTopDown()
                        .filter { it.isFile && it.name.equals("meta.json", ignoreCase = true) }
                        .firstOrNull()
                    var metaName: String? = null
                    var metaVendor: String? = null
                    var metaDriverVersion: String? = null
                    var metaLibraryName: String? = null
                    var metaMinApi: Int = 0
                    if (metaFile != null) {
                        runCatching {
                            val o = org.json.JSONObject(metaFile.readText())
                            metaName = o.optString("name", "").ifBlank { null }
                            metaVendor = o.optString("vendor", "").ifBlank { null }
                            metaDriverVersion = o.optString("driverVersion", "").ifBlank { null }
                            metaLibraryName = o.optString("libraryName", "").ifBlank { null }
                            metaMinApi = o.optInt("minApi", 0)
                            PX5EventLog.event("driverImport", "meta_json",
                                    "name=$metaName vendor=$metaVendor " +
                                    "driverVersion=$metaDriverVersion " +
                                    "libraryName=$metaLibraryName minApi=$metaMinApi")
                        }.onFailure { f ->
                            android.util.Log.w("PX5", "meta.json parse failed: ${f.message}")
                            PX5EventLog.exception("driverImport.metaJson", f)
                        }
                    } else {
                        PX5EventLog.event("driverImport", "meta_json",
                                "detail=absent (filename scan fallback)")
                    }

                    // Honest gate: a pack demanding a newer Android than this
                    // device cannot work — reject with the real reason.
                    if (metaMinApi > android.os.Build.VERSION.SDK_INT) {
                        resultMsg = "Driver requires Android API $metaMinApi " +
                                "(this device: ${android.os.Build.VERSION.SDK_INT}) — not installable here."
                        PX5EventLog.event("driverImport", "min_api_rejected",
                                "required=$metaMinApi device=${android.os.Build.VERSION.SDK_INT}")
                        launch(Dispatchers.Main) {
                            Toast.makeText(context, resultMsg, Toast.LENGTH_LONG).show()
                        }
                        return@launch
                    }

                    // Candidate 1: the exact library named by meta.json.
                    val metaNamedLib = metaLibraryName?.let { libName ->
                        dir.walkTopDown()
                            .filter { it.isFile && it.name == libName }
                            .maxByOrNull { it.length() }
                    }

                    // Candidate 2: any shared object whose name belongs to
                    // the Vulkan driver family (fallback for packs without
                    // meta.json). Largest file wins ties.
                    val vulkanLib = Regex("(?i)(?:lib)?vulkan[^/]*\\.so(\\.\\d+)*$")
                    val byName = metaNamedLib ?: dir.walkTopDown()
                        .filter { it.isFile && vulkanLib.containsMatchIn(it.name) }
                        .maxByOrNull { it.length() }

                    // Candidate 2: a lone AArch64 ELF .so under another name.
                    val soFiles = dir.walkTopDown()
                        .filter { it.isFile && it.name.endsWith(".so") }
                        .toList()
                    val aarch64Elf = { f: File ->
                        runCatching {
                            f.inputStream().use { s ->
                                val h = ByteArray(20)
                                var n = 0
                                while (n < 20) {
                                    val r = s.read(h, n, 20 - n)
                                    if (r < 0) break
                                    n += r
                                }
                                if (n != 20) return@runCatching false
                                h[0] == 0x7f.toByte() && h[1] == 'E'.code.toByte() &&
                                        h[2] == 'L'.code.toByte() && h[3] == 'F'.code.toByte() &&
                                        // e_machine at offset 18, little-endian = 183 (AArch64)
                                        ((h[18].toInt() and 0xFF) or (h[19].toInt() shl 8)) == 183
                            }
                        }.getOrDefault(false)
                    }
                    val soloElf = soFiles
                        .filter { it.name != "package.zip" && aarch64Elf(it) }
                        .toList()
                        .singleOrNull()

                    val foundSo = byName?.takeIf { aarch64Elf(it) } ?: soloElf
                    PX5EventLog.event("driverImport", "library_scan",
                            "metaNamed=${metaNamedLib?.name} byName=${byName?.name} " +
                                    "soloElf=${soloElf?.name} foundSo=${foundSo?.name}")

                    resultMsg = when {
                        byName != null && soloElf == null && !aarch64Elf(byName) -> {
                            PX5EventLog.event("driverImport", "rejected",
                                    "reason=${byName.name} not arm64-v8a ELF")
                            "${byName.name} is not an arm64-v8a shared object — rejected."
                        }
                        foundSo == null -> {
                            // Honest failure + real archive evidence so the
                            // user can tell exactly what was inside.
                            val listing = seenNames.take(6).joinToString(", ")
                                .ifEmpty { "(unreadable or empty archive)" }
                            PX5EventLog.event("driverImport", "no_library_found",
                                    "contents=$listing")
                            "No Vulkan driver library found in the archive — nothing registered.\n" +
                                    "Contents seen: $listing\n" +
                                    "Expected a Turnip/Mesa package (vulkan.turnip.so, libvulkan.so, " +
                                    "libvulkan_adreno.so) for arm64-v8a."
                        }
                        !aarch64Elf(foundSo) -> {
                            PX5EventLog.event("driverImport", "rejected",
                                    "reason=${foundSo.name} not arm64-v8a ELF")
                            "${foundSo.name} is not an arm64-v8a shared object — rejected."
                        }
                        else -> {
                            val wrapper = fexCoreWrapper
                            when {
                                wrapper == null ->
                                    "Driver extracted but the engine library is unavailable."
                                else -> {
                                    // meta.json path: the loader opens the file
                                    // under its OWN libraryName — the ecosystem
                                    // contract (Winlator/Eden do the same). Only
                                    // nameless packs get normalized to the
                                    // legacy libvulkan_adreno.so soname.
                                    val hasMeta = !metaLibraryName.isNullOrBlank()
                                    val soname: String
                                    val libForSlot: File
                                    if (hasMeta) {
                                        soname = foundSo.name
                                        libForSlot = foundSo
                                    } else {
                                        soname = "libvulkan_adreno.so"
                                        libForSlot = File(dir, soname)
                                        if (foundSo.absolutePath != libForSlot.absolutePath) {
                                            foundSo.copyTo(libForSlot, overwrite = true)
                                        }
                                    }

                                    val label = metaName
                                        ?: (metaVendor?.let { v -> "$v driver" }
                                            ?: "Driver ${dir.name}")
                                    // v1.36 — bundle the package's non-public
                                    // platform deps (libhardware.so etc.) next
                                    // to the driver .so, so the slot dir is
                                    // self-sufficient for BOTH the preload
                                    // namespace and the adrenotools hook at
                                    // vkCreateInstance.
                                    val bundledDeps = bundleDriverDeps(
                                        libForSlot, libForSlot.parentFile ?: dir)
                                    val slot = wrapper.nativeRegisterDriverSlot(
                                        label, libForSlot.absolutePath, soname
                                    )
                                    if (slot > 0) {
                                        wrapper.nativeSetDriverMode(slot)
                                        Px5Settings.setDriverMode(slot)
                                        DriverSlotStore.append(
                                            context,
                                            DriverSlotStore.Slot(
                                                label, libForSlot.absolutePath, soname)
                                        )
                                        PX5EventLog.event("driverImport", "installed",
                                                "slot=$slot label=$label soname=$soname " +
                                                        "path=${libForSlot.absolutePath} " +
                                                        "bundled=${bundledDeps.joinToString(",")} " +
                                                        "source=${if (hasMeta) "meta.json" else "filename-scan"}")
                                        val provenance = listOfNotNull(
                                            metaVendor, metaDriverVersion
                                        ).joinToString(" • ")
                                        "Driver installed • slot $slot active.\n" +
                                                label +
                                                (if (provenance.isNotBlank()) " ($provenance)" else "") +
                                                "\n${libForSlot.name} -> ${libForSlot.absolutePath}" +
                                                (if (bundledDeps.isNotEmpty())
                                                    "\n+${bundledDeps.size} platform dep(s) bundled: " +
                                                            bundledDeps.joinToString(", ") else "") +
                                                (if (hasMeta) "\nsource: meta.json (libraryName)" else
                                                    "\nsource: filename scan (no meta.json)")
                                    } else {
                                        PX5EventLog.event("driverImport", "registration_rejected",
                                                "label=$label soname=$soname")
                                        "Extraction ok but native slot registration rejected."
                                    }
                                }
                            }
                        }
                    }
                } catch (e: Exception) {
                    resultMsg = "Failed to import driver: ${e.message}"
                    PX5EventLog.exception("driverImport", e)
                }
                launch(Dispatchers.Main) {
                    // v1.47: the import finished AFTER the sheet's ON_RESUME
                    // observer already fired (picker resume beats the IO
                    // coroutine), so the only bump that carries the
                    // post-import truth is this one — nativeRegisterDriverSlot
                    // and DriverSlotStore.append are done by now.
                    driverStateEpoch++
                    Toast.makeText(context, resultMsg, Toast.LENGTH_LONG).show()
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
                    fexCoreWrapper = fexCoreWrapper,
                    onGameSelected = { path ->
                        navController.navigate("emulation?path=$path")
                    },
                    onOpenSettings = {
                        navController.navigate("settings")
                    },
                    onImportFileClick = {
                        importFileLauncher.launch(arrayOf("*/*"))
                    },
                    onImportFolderClick = {
                        importFolderLauncher.launch(null)
                    }
                )
            }

            composable("settings") {
                PS5SettingsScreen(
                    soundManager = soundManager,
                    fexCoreStatus = fexCoreStatus,
                    driverStateEpoch = driverStateEpoch,
                    fexCoreWrapper = fexCoreWrapper,
                    onImportFileClick = {
                        importFileLauncher.launch(arrayOf("*/*"))
                    },
                    onImportFolderClick = {
                        importFolderLauncher.launch(null)
                    },
                    onScanGamesClick = { scanStorage() },
                    onOpenTurnipManagerClick = { showTurnipManagerSheet = true },
                    onOpenLogsClick = { navController.navigate("logs") },
                    onBackClick = { navController.popBackStack() }
                )
            }

            composable("logs") {
                PS5LogsScreen(onBackClick = { navController.popBackStack() })
            }

            composable("emulation?path={path}") { backStackEntry ->
                val path = backStackEntry.arguments?.getString("path") ?: ""
                EmuScreen(
                    path = path,
                    gameViewModel = gameViewModel,
                    fexCoreStatus = fexCoreStatus,
                    fexCoreWrapper = fexCoreWrapper,
                    onBackClick = { navController.popBackStack() },
                    onOpenLogs = { navController.navigate("logs") }
                )
            }
        }

        // Honest import progress / result strip (real work, real outcome).
        if (importStatus != null) {
            ImportStatusCard(
                text = importStatus!!,
                busy = importBusy,
                onDismiss = { if (!importBusy) importStatus = null },
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .padding(16.dp)
            )
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
                        driverLauncher.launch(arrayOf("*/*"))
                    },
                    onStateChanged = { driverStateEpoch++ },
                    onDismiss = {
                        showTurnipManagerSheet = false
                        driverStateEpoch++  // final sync even if no change event fired
                    }
                )
            }
        }
    }
}

/**
 * Import status card showing progress or results of game/driver imports.
 * @param text Status text to display
 * @param busy Whether import is in progress
 * @param onDismiss Callback when user dismisses the card
 * @param modifier Modifier for the card
 */
@Composable
private fun ImportStatusCard(
    text: String,
    busy: Boolean,
    onDismiss: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(px5Colors().sheet.copy(alpha = 0.97f))
            .padding(16.dp)
    ) {
        Column {
            Text(
                text = if (busy) "Importing…" else "Import finished",
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
                color = if (busy) px5Colors().infoMono else px5Colors().success,
                fontFamily = TitilliumFontFamily
            )
            Text(
                text = text,
                fontSize = 12.sp,
                color = px5Colors().text.copy(alpha = 0.85f),
                fontFamily = TitilliumFontFamily,
                modifier = Modifier.padding(top = 6.dp, bottom = 8.dp)
            )
            if (!busy) {
                Button(
                    onClick = onDismiss,
                    colors = ButtonDefaults.buttonColors(
                        containerColor = px5Colors().controlStrong,
                        contentColor = px5Colors().text
                    ),
                    shape = RoundedCornerShape(10.dp)
                ) {
                    Text("Close", fontSize = 12.sp)
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// PhysicalControllerBridge — hardware gamepad pass-through.
//
// WHY THIS EXISTS: Android gaming handhelds (AYN Odin 2 class, Retroid
// Pocket class, Logitech G Cloud class) expose their built-in controls as
// standard Android GAMEPAD/JOYSTICK devices, and any Bluetooth pad behaves
// the same. This bridge routes those events into the SAME native input
// atomics the on-screen DualSense overlay drives (nativeSetButtonState /
// nativeSetLeftStick / nativeSetRightStick / nativeSetTriggers), so touch
// and physical input compose instead of competing. One mapping, two input
// surfaces, zero new native surface area.
//
// Enabled only while EmuScreen is on screen; every mapping follows the
// Android GameController conventions (AXIS_X/Y left stick, AXIS_Z/RZ right
// stick, AXIS_HAT_X/Y D-pad, BRAKE/LTRIGGER + GAS/RTRIGGER triggers).
// ---------------------------------------------------------------------------

/**
 * Physical gamepad bridge for Android gaming handhelds and Bluetooth controllers.
 * Routes hardware input to the same native atomics as on-screen touch controls.
 */
object PhysicalControllerBridge {

    @Volatile var enabled: Boolean = false
    @Volatile var wrapper: FexCoreWrapper? = null

    private const val DEADZONE = 0.08f

    // Sticky axis state: partial MotionEvents must never zero the other
    // stick (many pads emit one-axis-per-event batches).
    private var lx = 0f; private var ly = 0f
    private var rx = 0f; private var ry = 0f
    private var l2 = 0f; private var r2 = 0f

    /**
     * Applies deadzone to analog axis value.
     * @param v Input axis value
     * @return Deadzoned value (0 if within deadzone)
     */
    private fun dz(v: Float): Float = if (kotlin.math.abs(v) < DEADZONE) 0f else v

    /**
     * Clears every axis and button so leaving a game never sticks inputs.
     * Buttons are released explicitly: the native atomics latch state, so a
     * button held while this bridge is disabled stays pressed forever.
     */
    fun reset() {
        lx = 0f; ly = 0f; rx = 0f; ry = 0f; l2 = 0f; r2 = 0f
        val w = wrapper ?: return
        w.nativeSetLeftStick(0f, 0f)
        w.nativeSetRightStick(0f, 0f)
        w.nativeSetTriggers(0f, 0f)
        val buttons = intArrayOf(
            FexCoreWrapper.PAD_CROSS, FexCoreWrapper.PAD_CIRCLE,
            FexCoreWrapper.PAD_SQUARE, FexCoreWrapper.PAD_TRIANGLE,
            FexCoreWrapper.PAD_DPAD_UP, FexCoreWrapper.PAD_DPAD_DOWN,
            FexCoreWrapper.PAD_DPAD_LEFT, FexCoreWrapper.PAD_DPAD_RIGHT,
            FexCoreWrapper.PAD_L1, FexCoreWrapper.PAD_R1,
            FexCoreWrapper.PAD_OPTIONS, FexCoreWrapper.PAD_SHARE,
            FexCoreWrapper.PAD_PS_HOME, FexCoreWrapper.PAD_L3,
            FexCoreWrapper.PAD_R3,
        )
        for (bit in buttons) w.nativeSetButtonState(bit, false)
    }

    /**
     * Handles physical gamepad key events (buttons, D-pad).
     * @param e Key event
     * @return true if event was consumed, false otherwise
     */
    fun handleKey(e: KeyEvent): Boolean {
        val w = wrapper ?: return false
        val pressed = e.action == KeyEvent.ACTION_DOWN
        if (e.action != KeyEvent.ACTION_DOWN && e.action != KeyEvent.ACTION_UP) {
            return false
        }
        val bit = when (e.keyCode) {
            KeyEvent.KEYCODE_BUTTON_A     -> FexCoreWrapper.PAD_CROSS
            KeyEvent.KEYCODE_BUTTON_B     -> FexCoreWrapper.PAD_CIRCLE
            KeyEvent.KEYCODE_BUTTON_X     -> FexCoreWrapper.PAD_SQUARE
            KeyEvent.KEYCODE_BUTTON_Y     -> FexCoreWrapper.PAD_TRIANGLE
            KeyEvent.KEYCODE_DPAD_UP      -> FexCoreWrapper.PAD_DPAD_UP
            KeyEvent.KEYCODE_DPAD_DOWN    -> FexCoreWrapper.PAD_DPAD_DOWN
            KeyEvent.KEYCODE_DPAD_LEFT    -> FexCoreWrapper.PAD_DPAD_LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT   -> FexCoreWrapper.PAD_DPAD_RIGHT
            KeyEvent.KEYCODE_BUTTON_L1    -> FexCoreWrapper.PAD_L1
            KeyEvent.KEYCODE_BUTTON_R1    -> FexCoreWrapper.PAD_R1
            KeyEvent.KEYCODE_BUTTON_SELECT -> FexCoreWrapper.PAD_SHARE
            KeyEvent.KEYCODE_BUTTON_START -> FexCoreWrapper.PAD_OPTIONS
            KeyEvent.KEYCODE_BUTTON_MODE  -> FexCoreWrapper.PAD_PS_HOME
            else -> return false
        }
        return w.nativeSetButtonState(bit, pressed)
    }

    /**
     * Handles physical gamepad motion events (analog sticks, triggers, HAT D-pad).
     * @param e Motion event
     * @return true if event was consumed, false otherwise
     */
    fun handleMotion(e: MotionEvent): Boolean {
        val w = wrapper ?: return false
        val src = e.source
        val isJoystick = (src and android.view.InputDevice.SOURCE_CLASS_JOYSTICK) != 0
        val isGamepad = (src and android.view.InputDevice.SOURCE_GAMEPAD) != 0
        if (!isJoystick && !isGamepad) return false

        lx = dz(e.getAxisValue(MotionEvent.AXIS_X))
        ly = dz(e.getAxisValue(MotionEvent.AXIS_Y))
        rx = dz(e.getAxisValue(MotionEvent.AXIS_Z))
        ry = dz(e.getAxisValue(MotionEvent.AXIS_RZ))
        l2 = maxOf(e.getAxisValue(MotionEvent.AXIS_BRAKE),
                   e.getAxisValue(MotionEvent.AXIS_LTRIGGER))
        r2 = maxOf(e.getAxisValue(MotionEvent.AXIS_GAS),
                   e.getAxisValue(MotionEvent.AXIS_RTRIGGER))

        // HAT emulates a D-pad on pads that have no dedicated DPAD source.
        val hx = e.getAxisValue(MotionEvent.AXIS_HAT_X)
        val hy = e.getAxisValue(MotionEvent.AXIS_HAT_Y)
        w.nativeSetButtonState(FexCoreWrapper.PAD_DPAD_LEFT,  hx < -0.5f)
        w.nativeSetButtonState(FexCoreWrapper.PAD_DPAD_RIGHT, hx >  0.5f)
        w.nativeSetButtonState(FexCoreWrapper.PAD_DPAD_UP,    hy < -0.5f)
        w.nativeSetButtonState(FexCoreWrapper.PAD_DPAD_DOWN,  hy >  0.5f)

        w.nativeSetLeftStick(lx, ly)
        w.nativeSetRightStick(rx, ry)
        w.nativeSetTriggers(l2, r2)
        return true
    }
}

// ---------------------------------------------------------------------------
// v1.36 — GPU driver package dependency bundling.
//
// The 2026-09-02 vc36 session proved the wall for 2026-era AdrenoTools-style
// Turnip packages ("Turnip v26.3.0-R4"): the driver .so carries DT_NEEDED
// entries for NON-public platform libraries (libhardware.so). No app-visible
// linker namespace resolves those — a SHARED namespace shares only the
// public library list — so the preload failed AND the adrenotools hook's
// load at vkCreateInstance (whose namespace searches the slot dir only)
// would fall back to the system driver. Eden/Vita3K never hit this because
// their ecosystem packages are NDK-built and self-contained.
//
// Fix, matching what the system itself does for /vendor's vulkan.adreno.so:
// at import time we read the driver's DT_NEEDED set (ELF64 parser below),
// copy every non-core dependency from the device's own lib dirs into the
// slot dir — transitively, so the bundled libs' own needs are covered — and
// log exactly what was bundled. After this the slot dir is self-sufficient:
// both our verification namespace and the hook resolve all deps there.
// ---------------------------------------------------------------------------
private val kPlatformLibDirs = listOf(
    "/odm/lib64", "/vendor/lib64", "/system/vendor/lib64",
    "/system_ext/lib64", "/system/lib64"
)

// Bionic + the guaranteed-public libs every SHARED namespace already links.
// Never copy these: shadowing them is at best pointless, at worst harmful.
private val kNeverBundle = setOf(
    "libc.so", "libm.so", "libdl.so", "liblog.so", "libz.so",
    "libstdc++.so", "libc++.so", "libc++_shared.so",
    "libandroid.so", "libnativewindow.so", "libvulkan.so",
    "libEGL.so", "libGLESv2.so", "libGLESv1_CM.so"
)

private class Phdr(val type: Int, val offset: Long, val vaddr: Long, val filesz: Long)

private fun leU16(b: ByteArray, o: Int): Int =
    (b[o].toInt() and 0xFF) or ((b[o + 1].toInt() and 0xFF) shl 8)

private fun leU32(b: ByteArray, o: Int): Int =
    leU16(b, o) or (leU16(b, o + 2) shl 16)

private fun leU64(b: ByteArray, o: Int): Long =
    (leU32(b, o).toLong() and 0xFFFFFFFFL) or
            ((leU32(b, o + 4).toLong() and 0xFFFFFFFFL) shl 32)

// Reads the ELF64 program headers; empty list for anything that is not a
// little-endian ELF64 image (the import gate only accepts AArch64 anyway).
private fun readElf64Phdrs(raf: java.io.RandomAccessFile): List<Phdr> {
    val h = ByteArray(64)
    raf.seek(0)
    if (raf.read(h) != 64) return emptyList()
    if (h[0] != 0x7F.toByte() || h[1] != 'E'.code.toByte() ||
        h[2] != 'L'.code.toByte() || h[3] != 'F'.code.toByte()) return emptyList()
    if (h[4].toInt() != 2 || h[5].toInt() != 1) return emptyList()  // 64-bit LE
    val phOff = leU64(h, 0x20)
    val phEnt = leU16(h, 0x36)
    val phNum = leU16(h, 0x38)
    if (phEnt < 56 || phNum == 0 || phNum > 256) return emptyList()
    val buf = ByteArray(phEnt * phNum)
    raf.seek(phOff)
    if (raf.read(buf) != buf.size) return emptyList()
    return (0 until phNum).map { i ->
        val b = i * phEnt
        Phdr(
            leU32(buf, b),                      // p_type
            leU64(buf, b + 8),                  // p_offset
            leU64(buf, b + 0x10),               // p_vaddr
            leU64(buf, b + 0x20)                // p_filesz
        )
    }
}

private fun vaddrToFileOffset(phdrs: List<Phdr>, v: Long): Long? =
    phdrs.firstOrNull {
        it.type == 1 /*PT_LOAD*/ && v >= it.vaddr && v < it.vaddr + it.filesz
    }?.let { it.offset + (v - it.vaddr) }

// DT_NEEDED sonames of an ELF64 shared object (best effort; empty on any
// parse surprise — the bundler then simply bundles nothing).
private fun elfNeededLibraries(so: File): List<String> = runCatching {
    java.io.RandomAccessFile(so, "r").use { raf ->
        val phdrs = readElf64Phdrs(raf)
        if (phdrs.none { it.type == 1 }) return emptyList()
        val dyn = phdrs.firstOrNull { it.type == 2 /*PT_DYNAMIC*/ }
            ?: return emptyList()
        val dynBuf = ByteArray(minOf(dyn.filesz, 256L * 1024L).toInt())
        raf.seek(dyn.offset)
        raf.readFully(dynBuf)
        var strTabV = -1L
        val needed = mutableListOf<Long>()
        var i = 0
        while (i + 16 <= dynBuf.size) {
            when (leU64(dynBuf, i)) {
                0L -> break                                    // DT_NULL
                1L -> needed += leU64(dynBuf, i + 8)           // DT_NEEDED
                5L -> strTabV = leU64(dynBuf, i + 8)           // DT_STRTAB
            }
            i += 16
        }
        if (strTabV <= 0 || needed.isEmpty()) return emptyList()
        val strOff = vaddrToFileOffset(phdrs, strTabV) ?: return emptyList()
        val strLen = minOf(256L * 1024L, raf.length() - strOff).toInt()
        if (strLen <= 0) return emptyList()
        val strBuf = ByteArray(strLen)
        raf.seek(strOff)
        raf.readFully(strBuf)
        needed.mapNotNull { off ->
            if (off >= strBuf.size) return@mapNotNull null
            var end = off.toInt()
            while (end < strBuf.size && strBuf[end] != 0.toByte()) end++
            String(strBuf, off.toInt(), end - off.toInt(), Charsets.UTF_8)
                .takeIf { it.isNotBlank() }
        }
    }
}.getOrDefault(emptyList())

// Copies every missing non-core DT_NEEDED dependency of `driverSo` (and of
// each copied dependency, transitively) into `slotDir`, sourcing from the
// device's own lib dirs. Returns the bundled names for the event log.
private fun bundleDriverDeps(driverSo: File, slotDir: File): List<String> {
    val bundled = mutableListOf<String>()
    val seen = mutableSetOf(driverSo.name)
    val queue = ArrayDeque<File>()
    queue.addLast(driverSo)
    var guard = 0
    while (queue.isNotEmpty() && guard++ < 64) {
        val lib = queue.removeFirst()
        for (name in elfNeededLibraries(lib)) {
            if (name in kNeverBundle || !seen.add(name)) continue
            val src = kPlatformLibDirs.firstOrNull { d ->
                File(d, name).isFile
            }?.let { File(it, name) }
            if (src == null) {
                com.px5.emulator.core.PX5EventLog.event(
                    "driverImport", "dep_not_on_device",
                    "lib=$name needed_by=${lib.name}")
                continue
            }
            val dst = File(slotDir, name)
            if (!dst.isFile || dst.length() != src.length()) {
                src.copyTo(dst, overwrite = true)
            }
            bundled += name
            queue.addLast(dst)
        }
    }
    return bundled
}
