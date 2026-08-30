package com.px5.emulator

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Shader
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.OpenableColumns
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.BufferedInputStream
import java.io.ByteArrayInputStream
import java.io.DataInputStream
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.security.MessageDigest

/**
 * GameImporter — the REAL import pipeline for PS5 content.
 *
 * Formats accepted (no fake steps, no invented progress):
 *
 *   1. Dumped game folders (jailbroken-PS5 dump, exFAT drive dump, or a
 *      folder registered with sharpdroid / SharpEmu-arm64 — they all share
 *      the same contract): any directory containing "eboot.bin" (any case,
 *      depth <= 8). Metadata from sce_sys/param.json (titleId, localized
 *      title with defaultLanguage -> en-US -> first available fallback),
 *      cover from sce_sys/icon0.png -> pic0.png -> pic1.png. Sizes are
 *      real byte sums.
 *
 *   2. .pkg — REAL header parsing (big-endian, verified against the
 *      LibOrbisPkg reference):
 *        magic "\x7fCNT" @0x00, entry_count u32 @0x10,
 *        entry_table_offset u32 @0x18, body_offset u64 @0x20,
 *        body_size u64 @0x28, content_id char[0x30] @0x40.
 *      32-byte meta entries: id, nameOffset, flags1, flags2, dataOffset,
 *      dataSize, u64 pad; entry encrypted when (flags1 & 0x80000000) != 0.
 *      PARAM.SFO (id 0x1000) parsed when stored unencrypted -> TITLE /
 *      TITLE_ID / APP_VER. ICON0.PNG (0x1200 family) and PIC1.PNG
 *      (0x1006) extracted as cover when stored unencrypted and carrying a
 *      real PNG signature. The PFS game payload itself is per-title
 *      encrypted: registration says plainly that booting needs the
 *      decryption pipeline (Phase C) — nothing pretends otherwise.
 *
 *   3. .elf — ELF64 magic + x86-64 machine check; PS5 flavour detected
 *      via the ET_DYNEXEC/ET_DYNAMIC e_type values and recorded in
 *      status. .self containers are recognized and honestly rejected as
 *      "decryption not implemented" (same policy as KytyPS5 / SharpEmu).
 *
 *   4. .iso — ISO9660 signature check ("CD001" @0x8001), registered as a
 *      disc image; there is no UFS/PFS disc extraction yet and the status
 *      line says so.
 *
 * Content stays where it lives: SAF permissions are persisted and a real
 * filesystem path is stored when one is derivable. Only boot candidates
 * (.elf) are copied into app storage, because the native loader needs a
 * plain path — and boot files are small next to full dumps.
 */
object GameImporter {

    data class ImportReport(
        val imported: MutableList<String> = mutableListOf(),
        val skipped: MutableList<String> = mutableListOf()
    ) {
        fun summary(): String = buildString {
            append("Imported ${imported.size}")
            if (skipped.isNotEmpty()) append(", skipped ${skipped.size}")
            append(".")
            skipped.take(4).forEach { append("\n• $it") }
            if (skipped.size > 4) append("\n• … and ${skipped.size - 4} more")
        }
    }

    // ------------------------------------------------------------------
    // Public entry points
    // ------------------------------------------------------------------

    /** Import one picked SAF document (file or tree). */
    suspend fun importUri(
        context: Context,
        uri: Uri,
        add: suspend (GameEntity) -> Unit,
        onProgress: (String) -> Unit = {}
    ): ImportReport = withContext(Dispatchers.IO) {
        val seen = HashSet<String>()
        val dedupAdd: suspend (GameEntity) -> Unit = { g -> if (seen.add(g.id)) add(g) }
        if (DocumentsContract.isTreeUri(uri)) {
            importTree(context, uri, dedupAdd, onProgress)
        } else {
            importSingleDocument(context, uri, dedupAdd, onProgress)
        }
    }

    /**
     * Best-effort scan of public storage locations where PS5 dumps and
     * packages are commonly placed (Download, legacy PX5/Games, folders
     * registered with competing emulators that use the public dump
     * contract). Only locations that actually exist and are readable are
     * touched; the report is honest about everything else.
     */
    suspend fun scanStorage(
        context: Context,
        add: suspend (GameEntity) -> Unit,
        onProgress: (String) -> Unit = {}
    ): ImportReport = withContext(Dispatchers.IO) {
        val seen = HashSet<String>()
        val dedupAdd: suspend (GameEntity) -> Unit = { g -> if (seen.add(g.id)) add(g) }
        val report = ImportReport()
        val roots = buildList {
            add(File("/sdcard/PX5/Games"))
            add(File("/sdcard/Download"))
            add(File("/sdcard/Games"))
            add(File("/sdcard/Android/data/com.mircowuffwuff.sharpdroid"))
            add(File("/sdcard/Android/data/com.sharpemu.android"))
            context.getExternalFilesDir(null)?.parentFile?.let { add(File(it, "Games")) }
        }
        for (root in roots) {
            if (!root.exists() || !root.canRead()) continue
            onProgress("Scanning ${root.absolutePath} …")
            scanFileTree(context, root, depth = 0, report, dedupAdd)
        }
        report
    }

    // ------------------------------------------------------------------
    // Tree (folder) import — the dump contract
    // ------------------------------------------------------------------

    private suspend fun importTree(
        context: Context,
        treeUri: Uri,
        add: suspend (GameEntity) -> Unit,
        onProgress: (String) -> Unit
    ): ImportReport {
        val report = ImportReport()
        try {
            context.contentResolver.takePersistableUriPermission(
                treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
        } catch (_: SecurityException) {
            // Non-persistable grant — the in-memory import still works.
        }
        val rootDoc = DocumentsContract.buildDocumentUriUsingTree(
            treeUri, DocumentsContract.getTreeDocumentId(treeUri)
        )
        onProgress("Scanning ${queryDisplayName(context, rootDoc) ?: "folder"} …")
        scanSafTree(context, rootDoc, depth = 0, report, add)
        return report
    }

    // Title-dir names: CUSA/PPSA + 5 digits, optionally with the -app0
    // suffix the standard dump tools append (device log 2026-08-30 shows
    // PPSA02929-app0 on storage).
    private val GAME_DIR_ID = Regex("^(CUSA|PPSA)[0-9]{5}(-app0)?$", RegexOption.IGNORE_CASE)

    private data class NestedEboot(val child: SafChild, val rel: String)

    private suspend fun scanSafTree(
        context: Context,
        dirUri: Uri,
        depth: Int,
        report: ImportReport,
        add: suspend (GameEntity) -> Unit
    ) {
        if (depth > 8) return
        val children = listChildren(context, dirUri)
        if (children.isEmpty()) return

        // Anchor 1: this directory is itself a game dump.
        if (children.any { !it.isDir && it.name.equals("eboot.bin", true) }) {
            importDumpFolder(context, dirUri, children, report, add)
            return
        }
        // Anchor 2: CUSA/PPSA-named subdirectories are game roots. The
        // executable itself may sit below the title dir — decrypted/
        // eboot.bin is the standard dump-tool layout (device log
        // 2026-08-30) — so after the direct-children check a bounded
        // SAF search runs before the title dir is skipped.
        val named = children.filter { it.isDir && GAME_DIR_ID.matches(it.name) }
        for (child in named) {
            val sub = DocumentsContract.buildDocumentUriUsingTree(
                dirUri, DocumentsContract.getDocumentId(child.uri)
            )
            val subChildren = listChildren(context, sub)
            val direct = subChildren.firstOrNull {
                !it.isDir && it.name.equals("eboot.bin", true)
            }
            val nested = direct?.let { NestedEboot(it, it.name) }
                    ?: findNestedEbootSaf(context, sub, subChildren)
            if (nested != null) {
                importDumpFolder(context, sub, subChildren, report, add,
                        ebootOverride = nested.child, ebootRel = nested.rel)
            }
        }
        if (named.isNotEmpty()) return

        // Otherwise recurse (bounded) looking for deeper dumps; pick up
        // loose .pkg/.iso files along the way. A folder holding sce_sys/
        // is treated as a dump root even when its name is not
        // CUSA/PPSA-named — the executable may still sit one level down.
        for (child in children) {
            if (child.isDir) {
                val sub = DocumentsContract.buildDocumentUriUsingTree(
                    dirUri, DocumentsContract.getDocumentId(child.uri)
                )
                val subChildren = listChildren(context, sub)
                if (subChildren.any { it.isDir && it.name.equals("sce_sys", true) }) {
                    val direct = subChildren.firstOrNull {
                        !it.isDir && it.name.equals("eboot.bin", true)
                    }
                    val nested = direct?.let { NestedEboot(it, it.name) }
                            ?: findNestedEbootSaf(context, sub, subChildren)
                    if (nested != null) {
                        importDumpFolder(context, sub, subChildren, report, add,
                                ebootOverride = nested.child, ebootRel = nested.rel)
                        continue
                    }
                }
                scanSafTree(context, sub, depth + 1, report, add)
            } else if (child.name.endsWith(".pkg", true) || child.name.endsWith(".iso", true)) {
                importLooseDocument(context, child, report, add)
            }
        }
    }

    /**
     * Bounded SAF search for eboot.bin below a title dir (2 levels, max
     * 24 dirs). A dir named decrypted/ is tried first at each level — the
     * standard dump-tool layout. Returns the executable document plus its
     * path relative to the title dir, or null when the tree has none.
     */
    private fun findNestedEbootSaf(
        context: Context,
        titleDirUri: Uri,
        children: List<SafChild>
    ): NestedEboot? {
        var level = children.filter { it.isDir }
                .sortedBy { it.name.lowercase() }
                .sortedByDescending { it.name.equals("decrypted", true) }
        var visited = 0
        repeat(2) { depth ->
            val next = ArrayList<SafChild>()
            for (d in level) {
                if (visited++ >= 24) return null
                val sub = DocumentsContract.buildDocumentUriUsingTree(
                    titleDirUri, DocumentsContract.getDocumentId(d.uri)
                )
                val kids = listChildren(context, sub)
                kids.firstOrNull { !it.isDir && it.name.equals("eboot.bin", true) }
                        ?.let { return NestedEboot(it, "${d.name}/${it.name}") }
                if (depth == 0) next += kids.filter { it.isDir }
            }
            level = next.sortedBy { it.name.lowercase() }
                    .sortedByDescending { it.name.equals("decrypted", true) }
        }
        return null
    }

    private suspend fun importDumpFolder(
        context: Context,
        dirUri: Uri,
        children: List<SafChild>,
        report: ImportReport,
        add: suspend (GameEntity) -> Unit,
        ebootOverride: SafChild? = null,
        ebootRel: String = ""
    ) {
        val docId = DocumentsContract.getDocumentId(dirUri)
        val folderName = docId.substringAfterLast(':').substringAfterLast('/')
            .ifBlank { "game" }

        val sceSys = children.firstOrNull { it.isDir && it.name.equals("sce_sys", true) }
        var meta = ParamMeta()
        var iconBytes: ByteArray? = null
        if (sceSys != null) {
            val sceSysUri = DocumentsContract.buildDocumentUriUsingTree(
                dirUri, DocumentsContract.getDocumentId(sceSys.uri)
            )
            val sceSysChildren = listChildren(context, sceSysUri)
            meta = sceSysChildren.firstOrNull { it.name.equals("param.json", true) }
                ?.let { readDocumentBytes(context, it.uri) }
                ?.let { bytes -> runCatching { parseParamJson(JSONObject(stripBom(bytes))) }.getOrNull() }
                ?: ParamMeta()
            iconBytes = firstNonEmpty(
                coverCandidate(context, sceSysChildren, "icon0.png"),
                coverCandidate(context, sceSysChildren, "pic0.png"),
                coverCandidate(context, sceSysChildren, "pic1.png")
            )
        }
        val eboot = ebootOverride
                ?: children.firstOrNull { !it.isDir && it.name.equals("eboot.bin", true) }
        if (eboot == null) {
            report.skipped += "$folderName — no eboot.bin (folder tree searched)"
            return
        }

        val size = subtreeSize(context, dirUri, limitDirs = 512)
        val id = stableId(meta.titleId.ifBlank { folderName }, docId)
        val cover = storeCover(context, iconBytes, id, meta.name.ifBlank { folderName })
        val direct = directPathFromTreeUri(context, dirUri)

        add(
            GameEntity(
                id = id,
                name = meta.name.ifBlank { prettify(folderName) },
                titleId = meta.titleId,
                path = direct ?: dirUri.toString(),
                isFolder = true,
                format = "DUMP",
                version = meta.version,
                sizeBytes = size,
                coverPath = cover,
                status = (if (direct != null) "Ready" else "Ready (SAF location)") +
                        (if (ebootRel.isNotBlank() && ebootRel != "eboot.bin")
                            " — eboot: $ebootRel" else ""),
                installedAtMillis = System.currentTimeMillis()
            )
        )
        report.imported += meta.name.ifBlank { folderName }
    }

    // ------------------------------------------------------------------
    // Single documents (files)
    // ------------------------------------------------------------------

    private suspend fun importSingleDocument(
        context: Context,
        uri: Uri,
        add: suspend (GameEntity) -> Unit,
        onProgress: (String) -> Unit
    ): ImportReport {
        val report = ImportReport()
        val name = queryDisplayName(context, uri) ?: "unnamed"
        onProgress("Reading $name …")
        val child = SafChild(name, querySize(context, uri) ?: 0L, false, uri)
        importLooseDocument(context, child, report, add)
        return report
    }

    private suspend fun importLooseDocument(
        context: Context,
        child: SafChild,
        report: ImportReport,
        add: suspend (GameEntity) -> Unit
    ) {
        val name = child.name
        val lower = name.lowercase()
        val size = child.size.takeIf { it > 0 } ?: documentSize(context, child.uri)
        val sized = child.copy(size = size)
        when {
            lower.endsWith(".pkg") -> importPkg(context, sized, report, add)
            lower.endsWith(".iso") -> importIso(context, sized, report, add)
            lower.endsWith(".elf") || lower.endsWith(".bin") || lower.endsWith(".oelf") ->
                importElf(context, sized, report, add)
            lower.endsWith(".self") || lower.endsWith(".sprx") ->
                importSelf(context, sized, report, add)
            else -> report.skipped += "$name — unsupported extension"
        }
    }

    private suspend fun importPkg(
        context: Context,
        child: SafChild,
        report: ImportReport,
        add: suspend (GameEntity) -> Unit
    ) {
        val info = context.contentResolver.openInputStream(child.uri)?.use { ins ->
            parsePkgHeader(BufferedInputStream(ins))
        }
        if (info == null) {
            report.skipped += "${child.name} — not a PS4/PS5 package (bad header magic)"
            return
        }
        val titleId = info.sfo["TITLE_ID"] ?: titleIdFromContentId(info.contentId)
        val name = info.sfo["TITLE"]
            ?: prettify(titleIdFromContentId(info.contentId).ifBlank { child.name })
        val id = stableId(titleId.ifBlank { name }, info.contentId.ifBlank { child.name })
        val cover = storeCover(context, info.iconPng, id, name)
        val direct = directPathFromTreeUri(context, child.uri)
        add(
            GameEntity(
                id = id,
                name = name,
                titleId = titleId,
                path = direct ?: child.uri.toString(),
                isFolder = false,
                format = "PKG",
                version = info.sfo["APP_VER"] ?: "",
                sizeBytes = child.size,
                coverPath = cover,
                status = "PKG registered — payload decryption pending",
                installedAtMillis = System.currentTimeMillis()
            )
        )
        report.imported += "$name (PKG${if (titleId.isNotBlank()) " • $titleId" else ""})"
    }

    private suspend fun importIso(
        context: Context,
        child: SafChild,
        report: ImportReport,
        add: suspend (GameEntity) -> Unit
    ) {
        val isIso = context.contentResolver.openInputStream(child.uri)?.use { ins ->
            val buf = ByteArray(0x8006)
            readFully(ins, buf)
            String(buf, 0x8001, 5, Charsets.US_ASCII) == "CD001"
        } ?: false
        if (!isIso) {
            report.skipped += "${child.name} — not an ISO9660 image"
            return
        }
        val base = child.name.substringBeforeLast('.')
        val direct = directPathFromTreeUri(context, child.uri)
        add(
            GameEntity(
                id = stableId(base, child.uri.toString()),
                name = prettify(base),
                path = direct ?: child.uri.toString(),
                isFolder = false,
                format = "ISO",
                sizeBytes = child.size,
                status = "Disc image registered — extraction pending",
                installedAtMillis = System.currentTimeMillis()
            )
        )
        report.imported += "${prettify(base)} (ISO)"
    }

    private suspend fun importElf(
        context: Context,
        child: SafChild,
        report: ImportReport,
        add: suspend (GameEntity) -> Unit
    ) {
        val head = context.contentResolver.openInputStream(child.uri)?.use { ins ->
            val buf = ByteArray(64)
            if (!readFully(ins, buf)) null else buf
        }
        if (head == null || head[0] != 0x7F.toByte() || head[1] != 'E'.code.toByte() ||
            head[2] != 'L'.code.toByte() || head[3] != 'F'.code.toByte()
        ) {
            report.skipped += "${child.name} — not an ELF image"
            return
        }
        val eType = ((head[17].toInt() and 0xFF) shl 8) or (head[16].toInt() and 0xFF)
        val eMachine = ((head[19].toInt() and 0xFF) shl 8) or (head[18].toInt() and 0xFF)
        if (eMachine != 62) { // EM_X86_64
            report.skipped += "${child.name} — ELF but not x86-64 (machine=$eMachine)"
            return
        }
        val isPs5 = eType == 0xFE10 || eType == 0xFE18
        val base = child.name.substringBeforeLast('.')

        // Copy boot candidates into app storage: the native loader wants a
        // plain filesystem path, and boot files are small next to dumps.
        val libDir = File(context.filesDir, "library").apply { mkdirs() }
        val dest = File(libDir, "${stableId(base, child.uri.toString())}_${child.name}")
        context.contentResolver.openInputStream(child.uri)?.use { ins ->
            FileOutputStream(dest).use { out -> ins.copyTo(out) }
        }
        add(
            GameEntity(
                id = stableId(base, child.uri.toString()),
                name = prettify(base),
                path = dest.absolutePath,
                isFolder = false,
                format = "ELF",
                sizeBytes = dest.length(),
                status = if (isPs5) "Ready" else "Generic x86-64 ELF",
                installedAtMillis = System.currentTimeMillis()
            )
        )
        report.imported += "${prettify(base)} (ELF${if (isPs5) " • PS5" else ""})"
    }

    private suspend fun importSelf(
        context: Context,
        child: SafChild,
        report: ImportReport,
        add: suspend (GameEntity) -> Unit
    ) {
        val head = context.contentResolver.openInputStream(child.uri)?.use { ins ->
            val buf = ByteArray(4)
            if (!readFully(ins, buf)) null else buf
        } ?: return
        val magic = ((head[0].toLong() and 0xFF) shl 24) or ((head[1].toLong() and 0xFF) shl 16) or
                ((head[2].toLong() and 0xFF) shl 8) or (head[3].toLong() and 0xFF)
        if (magic != 0x4F153D1DL && magic != 0x5414F5EEL) {
            report.skipped += "${child.name} — not a SELF container"
            return
        }
        report.skipped +=
            "${child.name} — SELF container: encrypted, decryption not implemented (supply a decrypted eboot.bin)"
    }

    // ------------------------------------------------------------------
    // PKG header parsing (reference: LibOrbisPkg — big-endian layout)
    // ------------------------------------------------------------------

    data class PkgHeaderInfo(
        val contentId: String,
        val bodyOffset: Long,
        val bodySize: Long,
        val entryCount: Int,
        val sfo: Map<String, String>,
        val iconPng: ByteArray?
    )

    data class PkgMetaEntry(
        val id: Long, val flags1: Long, val dataOffset: Long, val dataSize: Long
    ) {
        val encrypted: Boolean get() = (flags1 and 0x80000000L) != 0L
    }

    private const val PKG_ENTRY_PARAM_SFO = 0x1000L
    private const val PKG_ENTRY_PIC1_PNG = 0x1006L
    private const val PKG_ENTRY_ICON0_FIRST = 0x1200L
    private const val PKG_ENTRY_ICON0_LAST = 0x12FFL

    private const val PKG_FIXED_HEADER_SIZE = 0x1000

    /**
     * Parses the PKG header from a stream, tracking the absolute stream
     * position across skips so PARAM.SFO and icon entries are read from
     * the correct offsets no matter where the entry table sits.
     */
    private fun parsePkgHeader(ins: InputStream): PkgHeaderInfo? {
        val be = DataInputStream(BufferedInputStream(ins))
        val header = ByteArray(PKG_FIXED_HEADER_SIZE)
        if (!readFully(be, header)) return null
        if (header[0] != 0x7F.toByte() || header[1] != 'C'.code.toByte() ||
            header[2] != 'N'.code.toByte() || header[3] != 'T'.code.toByte()
        ) return null

        fun u32(off: Int): Long {
            var v = 0L
            for (i in 0 until 4) v = (v shl 8) or (header[off + i].toLong() and 0xFF)
            return v
        }
        fun u64(off: Int): Long {
            var v = 0L
            for (i in 0 until 8) v = (v shl 8) or (header[off + i].toLong() and 0xFF)
            return v
        }

        val entryCount = u32(0x10).toInt()
        val entryTableOffset = u32(0x18)
        val bodyOffset = u64(0x20)
        val bodySize = u64(0x28)
        val contentId = header.sliceArray(0x40 until 0x70)
            .takeWhile { it != 0.toByte() }
            .toByteArray()
            .toString(Charsets.US_ASCII)

        if (entryCount !in 1..4096) {
            return PkgHeaderInfo(contentId, bodyOffset, bodySize, 0, emptyMap(), null)
        }

        var pos = PKG_FIXED_HEADER_SIZE.toLong()   // absolute stream position now
        val metaBytes: ByteArray
        val metaBufOffset: Int
        if (entryTableOffset >= pos) {
            if (!skipFully(be, entryTableOffset - pos)) {
                return PkgHeaderInfo(contentId, bodyOffset, bodySize, 0, emptyMap(), null)
            }
            pos = entryTableOffset
            val buf = ByteArray(entryCount * 32)
            if (!readFully(be, buf)) {
                return PkgHeaderInfo(contentId, bodyOffset, bodySize, 0, emptyMap(), null)
            }
            pos += buf.size
            metaBytes = buf
            metaBufOffset = 0
        } else {
            // Entry table sits inside the fixed header slice already read.
            metaBytes = header
            metaBufOffset = entryTableOffset.toInt()
        }
        val entries = ArrayList<PkgMetaEntry>(entryCount)
        for (i in 0 until entryCount) {
            val off = metaBufOffset + i * 32
            if (off + 32 > metaBytes.size) break
            fun mu32(o: Int): Long =
                ((metaBytes[o].toLong() and 0xFF) shl 24) or ((metaBytes[o + 1].toLong() and 0xFF) shl 16) or
                        ((metaBytes[o + 2].toLong() and 0xFF) shl 8) or (metaBytes[o + 3].toLong() and 0xFF)
            entries += PkgMetaEntry(mu32(off), mu32(off + 8), mu32(off + 16), mu32(off + 20))
        }

        // Targeted reads: PARAM.SFO, then an unencrypted cover entry.
        fun readEntry(e: PkgMetaEntry): ByteArray? {
            if (e.dataSize <= 0 || e.dataSize > 64L * 1024 * 1024) return null
            if (e.dataOffset < pos) {
                // Entry data before our stream position: read from the
                // in-memory header slice when it fits there.
                if (e.dataOffset + e.dataSize <= PKG_FIXED_HEADER_SIZE) {
                    return header.sliceArray(
                        e.dataOffset.toInt() until (e.dataOffset + e.dataSize).toInt()
                    )
                }
                return null
            }
            if (!skipFully(be, e.dataOffset - pos)) return null
            val buf = ByteArray(e.dataSize.toInt())
            if (!readFully(be, buf)) return null
            pos = e.dataOffset + e.dataSize
            return buf
        }

        val sfo = entries.firstOrNull { it.id == PKG_ENTRY_PARAM_SFO && !it.encrypted }
            ?.let { e -> readEntry(e)?.let { parseSfo(it) } } ?: emptyMap()

        val iconPng = entries
            .filter { !it.encrypted && it.dataSize in 1..(16L * 1024 * 1024) }
            .sortedBy { it.id == PKG_ENTRY_PIC1_PNG }   // ICON0 family first
            .firstOrNull {
                it.id in PKG_ENTRY_ICON0_FIRST..PKG_ENTRY_ICON0_LAST || it.id == PKG_ENTRY_PIC1_PNG
            }
            ?.let { e -> readEntry(e)?.takeIf { isPng(it) } }

        return PkgHeaderInfo(contentId, bodyOffset, bodySize, entryCount, sfo, iconPng)
    }

    // ------------------------------------------------------------------
    // PARAM.SFO (reference: LibOrbisPkg SFO parser, little-endian)
    // ------------------------------------------------------------------

    fun parseSfo(bytes: ByteArray): Map<String, String> {
        if (bytes.size < 0x14) return emptyMap()
        val beMagic = ((bytes[0].toLong() and 0xFF) shl 24) or ((bytes[1].toLong() and 0xFF) shl 16) or
                ((bytes[2].toLong() and 0xFF) shl 8) or (bytes[3].toLong() and 0xFF)
        if (beMagic != 0x00505346L) return emptyMap() // "\0PSF" read big-endian

        fun le32(off: Int): Int =
            (bytes[off].toInt() and 0xFF) or ((bytes[off + 1].toInt() and 0xFF) shl 8) or
                    ((bytes[off + 2].toInt() and 0xFF) shl 16) or ((bytes[off + 3].toInt() and 0xFF) shl 24)
        fun le16(off: Int): Int =
            (bytes[off].toInt() and 0xFF) or ((bytes[off + 1].toInt() and 0xFF) shl 8)

        val keyTableStart = le32(0x08)
        val dataTableStart = le32(0x0C)
        val count = le32(0x10)
        val out = HashMap<String, String>()
        for (i in 0 until count) {
            val base = 0x14 + i * 0x10
            if (base + 0x10 > bytes.size) break
            val keyOffset = le16(base)
            val format = le16(base + 2)
            val len = le32(base + 4)
            val dataOffset = le32(base + 12)
            val keyStart = keyTableStart + keyOffset
            if (keyStart >= bytes.size) continue
            val key = bytes.copyOfRange(keyStart, bytes.size)
                .takeWhile { it != 0.toByte() }
                .toByteArray().toString(Charsets.UTF_8)
            val dataStart = dataTableStart + dataOffset
            if (dataStart >= bytes.size) continue
            when (format) {
                0x0404 -> { // Integer — none of the fields we need
                }
                0x0204 -> { // UTF-8 (null-terminated)
                    val end = minOf(dataStart + (len - 1).coerceAtLeast(0), bytes.size)
                    if (end > dataStart) out[key] = String(bytes, dataStart, end - dataStart, Charsets.UTF_8)
                }
                0x0400 -> { // UTF-8 special
                    val end = minOf(dataStart + len, bytes.size)
                    if (end > dataStart) out[key] = String(bytes, dataStart, end - dataStart, Charsets.UTF_8)
                }
            }
        }
        return out
    }

    // ------------------------------------------------------------------
    // param.json (dump metadata; fallback chain as SharpEmu implements it)
    // ------------------------------------------------------------------

    data class ParamMeta(val name: String = "", val titleId: String = "", val version: String = "")

    fun parseParamJson(json: JSONObject): ParamMeta {
        val titleId = json.optString("titleId", "")
        val version = json.optString("contentVersion", "")
            .ifBlank { json.optString("masterVersion", "") }
        return ParamMeta(pickLocalizedTitle(json) ?: "", titleId, version)
    }

    private fun pickLocalizedTitle(json: JSONObject): String? {
        fun fromLocalized(obj: JSONObject?): String? {
            val lp = obj?.optJSONObject("localizedParameters") ?: return null
            val default = lp.optString("defaultLanguage", "")
            if (default.isNotBlank()) {
                lp.optJSONObject(default)?.optString("titleName", "")?.takeIf { it.isNotBlank() }
                    ?.let { return it }
            }
            lp.optJSONObject("en-US")?.optString("titleName", "")?.takeIf { it.isNotBlank() }
                ?.let { return it }
            for (lang in lp.keys()) {
                lp.optJSONObject(lang)?.optString("titleName", "")?.takeIf { it.isNotBlank() }
                    ?.let { return it }
            }
            return null
        }
        return fromLocalized(json) ?: fromLocalized(json.optJSONObject("disc"))
    }

    // ------------------------------------------------------------------
    // Filesystem-side scanning (public storage, best effort)
    // ------------------------------------------------------------------

    private suspend fun scanFileTree(
        context: Context,
        root: File,
        depth: Int,
        report: ImportReport,
        add: suspend (GameEntity) -> Unit
    ) {
        if (depth > 6) return
        val entries = root.listFiles() ?: return
        if (entries.any { it.isFile && it.name.equals("eboot.bin", true) }) {
            // v1.19: the dump root may sit above this dir — dump tools land
            // decrypted/eboot.bin below the folder that holds sce_sys.
            // Climb while the parent keeps the dump metadata so the stored
            // library path and the param/icon lookups point at the real
            // root; the boot screen re-locates the executable itself.
            var gameRoot = root
            var up = root.parentFile
            while (up != null && depth > 0 && File(up, "sce_sys").isDirectory) {
                gameRoot = up
                up = up.parentFile
            }
            importDumpFileFolder(context, gameRoot, report, add)
            return
        }
        for (f in entries) {
            when {
                f.isDirectory -> scanFileTree(context, f, depth + 1, report, add)
                f.isFile && f.name.endsWith(".pkg", true) ->
                    importPkg(
                        context,
                        SafChild(f.name, f.length(), false, Uri.fromFile(f)), report, add
                    )
                f.isFile && f.name.endsWith(".iso", true) ->
                    importIso(
                        context,
                        SafChild(f.name, f.length(), false, Uri.fromFile(f)), report, add
                    )
            }
        }
    }

    private suspend fun importDumpFileFolder(
        context: Context,
        dir: File,
        report: ImportReport,
        add: suspend (GameEntity) -> Unit
    ) {
        val paramFile = File(dir, "sce_sys/param.json")
        val meta = if (paramFile.isFile) {
            runCatching { parseParamJson(JSONObject(stripBom(paramFile.readBytes()))) }.getOrNull()
        } else null
        val icon = firstNonEmpty(
            firstReadablePng(dir, "icon0.png"),
            firstReadablePng(dir, "pic0.png"),
            firstReadablePng(dir, "pic1.png")
        )
        val size = dir.walkTopDown().filter { it.isFile }.take(20000).sumOf { it.length() }
        val folderName = dir.name
        val id = stableId(meta?.titleId?.ifBlank { folderName } ?: folderName, dir.absolutePath)
        val cover = storeCover(context, icon, id, meta?.name?.ifBlank { folderName } ?: folderName)
        add(
            GameEntity(
                id = id,
                name = meta?.name?.ifBlank { prettify(folderName) } ?: prettify(folderName),
                titleId = meta?.titleId ?: "",
                path = dir.absolutePath,
                isFolder = true,
                format = "DUMP",
                version = meta?.version ?: "",
                sizeBytes = size,
                coverPath = cover,
                status = "Ready",
                installedAtMillis = System.currentTimeMillis()
            )
        )
        report.imported += meta?.name?.ifBlank { folderName } ?: folderName
    }

    private fun firstReadablePng(dir: File, fileName: String): ByteArray? {
        val file = dir.walkTopDown().take(2000)
            .firstOrNull { it.isFile && it.name.equals(fileName, true) } ?: return null
        return file.takeIf { it.length() in 1..(16L * 1024 * 1024) }?.readBytes()
            ?.takeIf { isPng(it) }
    }

    // ------------------------------------------------------------------
    // SAF helpers
    // ------------------------------------------------------------------

    data class SafChild(val name: String, val size: Long, val isDir: Boolean, val uri: Uri)

    private fun listChildren(context: Context, dirUri: Uri): List<SafChild> {
        val out = ArrayList<SafChild>()
        try {
            val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                dirUri, DocumentsContract.getDocumentId(dirUri)
            )
            context.contentResolver.query(
                childrenUri,
                arrayOf(
                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE,
                    DocumentsContract.Document.COLUMN_SIZE
                ),
                null, null, null
            )?.use { c ->
                while (c.moveToNext()) {
                    val docId = c.getString(0) ?: continue
                    val name = c.getString(1) ?: continue
                    val mime = c.getString(2) ?: ""
                    val size = if (c.isNull(3)) 0L else c.getLong(3)
                    out += SafChild(
                        name = name,
                        size = size,
                        isDir = mime == DocumentsContract.Document.MIME_TYPE_DIR,
                        uri = DocumentsContract.buildDocumentUriUsingTree(dirUri, docId)
                    )
                }
            }
        } catch (_: Exception) {
        }
        return out
    }

    private fun subtreeSize(context: Context, dirUri: Uri, limitDirs: Int): Long {
        var total = 0L
        var visited = 0
        val queue = ArrayDeque<Uri>()
        queue += dirUri
        while (queue.isNotEmpty() && visited < limitDirs) {
            val dir = queue.removeFirst()
            visited++
            for (c in listChildren(context, dir)) {
                if (c.isDir) queue += DocumentsContract.buildDocumentUriUsingTree(
                    dir, DocumentsContract.getDocumentId(c.uri)
                ) else total += c.size
            }
        }
        return total
    }

    private fun coverCandidate(
        context: Context,
        children: List<SafChild>,
        fileName: String
    ): ByteArray? {
        val child = children.firstOrNull { it.name.equals(fileName, true) } ?: return null
        return readDocumentBytes(context, child.uri)?.takeIf { isPng(it) }
    }

    private fun readDocumentBytes(context: Context, uri: Uri, max: Long = 16L * 1024 * 1024): ByteArray? =
        runCatching {
            context.contentResolver.openInputStream(uri)?.use { ins ->
                val bytes = ins.readBytes()
                if (bytes.size <= max) bytes else null
            }
        }.getOrNull()

    private fun documentSize(context: Context, uri: Uri): Long = querySize(context, uri) ?: 0L

    private fun queryDisplayName(context: Context, uri: Uri): String? =
        runCatching {
            context.contentResolver.query(
                uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null
            )?.use { c -> if (c.moveToFirst()) c.getString(0) else null }
        }.getOrNull()

    private fun querySize(context: Context, uri: Uri): Long? =
        runCatching {
            context.contentResolver.query(
                uri, arrayOf(OpenableColumns.SIZE), null, null, null
            )?.use { c -> if (c.moveToFirst() && !c.isNull(0)) c.getLong(0) else null }
        }.getOrNull()

    /**
     * Translate a SAF tree/document URI under externalstorage into a real
     * filesystem path when possible ("/tree/primary:Games" ->
     * "/storage/emulated/0/Games"). Returns null when the mapping does not
     * apply or the path is not readable.
     */
    private fun directPathFromTreeUri(context: Context, uri: Uri): String? = runCatching {
        val docId = DocumentsContract.getDocumentId(uri)      // "primary:Games/CUSA12345"
        val treePart = DocumentsContract.getTreeDocumentId(uri)
        val root = treePart.substringBefore(':')
        val rest = docId.substringAfter(':', "").ifBlank { treePart.substringAfter(':', "") }
        if (root != "primary") return@runCatching null
        val candidate = if (rest.isBlank()) "/storage/emulated/0" else "/storage/emulated/0/$rest"
        File(candidate).takeIf { it.exists() && it.canRead() }?.absolutePath
    }.getOrNull()

    // ------------------------------------------------------------------
    // Cover storage / generation
    // ------------------------------------------------------------------

    /** Copy a real cover or generate a deterministic fallback cover. */
    fun storeCover(context: Context, png: ByteArray?, gameId: String, name: String): String {
        return try {
            val covers = File(context.filesDir, "covers").apply { mkdirs() }
            val dest = File(covers, "$gameId.png")
            if (png != null && isPng(png)) {
                dest.writeBytes(png)
            } else {
                generateCover(name, gameId)
                    .compress(Bitmap.CompressFormat.PNG, 90, dest.outputStream())
            }
            dest.absolutePath
        } catch (_: Throwable) {
            ""
        }
    }

    private fun generateCover(name: String, seedKey: String): Bitmap {
        val w = 512
        val h = 768
        val seed = MessageDigest.getInstance("MD5").digest(seedKey.toByteArray())
        val hue1 = (seed[0].toInt() and 0xFF) / 255f
        val hue2 = (hue1 + 0.25f + (seed[1].toInt() and 0x3F) / 255f) % 1f
        val c1 = android.graphics.Color.HSVToColor(floatArrayOf(hue1 * 360f, 0.65f, 0.45f))
        val c2 = android.graphics.Color.HSVToColor(floatArrayOf(hue2 * 360f, 0.60f, 0.20f))
        val bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(bmp)
        val paint = Paint().apply {
            shader = LinearGradient(0f, 0f, w.toFloat(), h.toFloat(), c1, c2, Shader.TileMode.CLAMP)
        }
        canvas.drawRect(0f, 0f, w.toFloat(), h.toFloat(), paint)
        val text = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0xCCFFFFFF.toInt()
            textSize = w / 4f
            textAlign = Paint.Align.CENTER
            isFakeBoldText = true
        }
        val sub = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            color = 0x99FFFFFF.toInt()
            textSize = w / 14f
            textAlign = Paint.Align.CENTER
        }
        canvas.drawText(name.take(2).uppercase(), w / 2f, h / 2f, text)
        canvas.drawText(name.take(28), w / 2f, h / 2f + w / 8f, sub)
        return bmp
    }

    // ------------------------------------------------------------------
    // Small utilities
    // ------------------------------------------------------------------

    fun stableId(primary: String, secondary: String): String {
        val raw = (primary.ifBlank { secondary }).lowercase()
            .replace(Regex("[^a-z0-9._-]"), "_")
        val digest = MessageDigest.getInstance("MD5").digest(secondary.toByteArray())
        val suffix = digest.take(4).joinToString("") { "%02x".format(it) }
        return "${raw.take(40)}-$suffix"
    }

    fun prettify(name: String): String =
        name.replace('_', ' ').replace('-', ' ').replace('.', ' ').trim()

    private fun titleIdFromContentId(contentId: String): String {
        // e.g. UP0001-PPSA01234_00-0000... -> PPSA01234
        return Regex("([CP]USA[0-9]{5})").find(contentId)?.groupValues?.get(1) ?: ""
    }

    private fun stripBom(bytes: ByteArray): String =
        String(bytes, Charsets.UTF_8).removePrefix("\uFEFF")

    private fun isPng(bytes: ByteArray): Boolean =
        bytes.size > 8 && bytes[0] == 0x89.toByte() && bytes[1] == 'P'.code.toByte() &&
                bytes[2] == 'N'.code.toByte() && bytes[3] == 'G'.code.toByte()

    private fun firstNonEmpty(vararg arrays: ByteArray?): ByteArray? =
        arrays.firstOrNull { it != null && it.isNotEmpty() }

    private fun readFully(ins: InputStream, buf: ByteArray): Boolean {
        var off = 0
        while (off < buf.size) {
            val n = ins.read(buf, off, buf.size - off)
            if (n < 0) return off == buf.size
            off += n
        }
        return true
    }

    private fun skipFully(ins: InputStream, count: Long): Boolean {
        var remaining = count
        val scratch = ByteArray(1)
        while (remaining > 0) {
            val skipped = ins.skip(remaining)
            if (skipped > 0) {
                remaining -= skipped
                continue
            }
            // Streams whose skip() is a no-op fall back to reading through.
            val n = ins.read(scratch, 0, 1)
            if (n <= 0) return false
            remaining -= n
        }
        return true
    }
}
