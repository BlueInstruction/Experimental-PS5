package com.px5.emulator

import java.io.File

/**
 * Recursive, bounded eboot.bin locator for dump folders.
 *
 * Root cause this fixes (device log 2026-08-30, v1.18 session): dump tools
 * commonly land the executable one or more levels below the game root —
 * `decrypted/eboot.bin` is the standard layout — while every previous
 * probe (boot report, load button, importer anchors) looked only at the
 * direct children. An honest ABSENT then blocked a loadable dump:
 * `exec_load_started target=` (empty) → `LOAD FAILED: no eboot.bin in folder`.
 *
 * Contract:
 *  - BFS, so the shallowest match always wins: a root-level eboot.bin
 *    beats a nested one; a depth-2 match beats depth-3.
 *  - Name match is case-insensitive (`eboot.bin`, `EBOOT.BIN`).
 *  - At equal depth a directory named `decrypted` is searched first —
 *    the common dump-tool layout — everything else alphabetical, so the
 *    result is deterministic across runs.
 *  - Depth and visited-directory caps bound the walk: subdirectories are
 *    enqueued down to depth [MAX_DEPTH] and the executable is matched among
 *    each visited level's files — a pathological or network-backed tree
 *    can never hang the caller (the probe runs on the UI thread's
 *    composition path today).
 *  - [Found.relPath] names where the file was found, for the boot report
 *    and the `exec_target` event — the report never fabricates, it names.
 */
object EbootLocator {

    /** Maximum folder depth searched below [find]'s root (root = depth 0). */
    const val MAX_DEPTH = 4

    /** Maximum directories visited per search — the UI-thread safety cap. */
    const val MAX_DIRS = 96

    data class Found(
        val file: File,
        /** Path relative to the search root, e.g. `decrypted/eboot.bin`. */
        val relPath: String,
        /** Depth at which the file was found (1 = direct child of root). */
        val depth: Int,
        /** Directories actually visited before the hit. */
        val dirsVisited: Int
    )

    /**
     * v1.27 — enumeration evidence. A "no eboot.bin" verdict that cannot
     * show these numbers is indistinguishable from a listing that silently
     * failed (scoped-storage listFiles() null), which is exactly how the
     * device sessions of 2026-08-30/31 could not arbitrate the user's
     * contradiction: their file listing showed eboot.bin at the game root
     * while every on-device report said ABSENT.
     */
    data class Stats(
        /** Directories dequeued, including ones whose listing failed. */
        val dirsWalked: Int,
        /** Child entries actually seen across successful listings. */
        val entriesSeen: Int,
        /** Directories whose listFiles() returned null (no read access). */
        val unreadableDirs: Int
    )

    data class Outcome(val found: Found?, val stats: Stats)

    fun search(root: File, maxDepth: Int = MAX_DEPTH, maxDirs: Int = MAX_DIRS): Outcome {
        if (!root.isDirectory) return Outcome(null, Stats(0, 0, 0))
        data class Entry(val dir: File, val depth: Int, val rel: String)
        val queue = ArrayDeque<Entry>()
        queue.add(Entry(root, 0, ""))
        var walked = 0
        var seen = 0
        var unreadable = 0
        while (queue.isNotEmpty()) {
            val (dir, depth, rel) = queue.removeFirst()
            walked++
            val children = runCatching { dir.listFiles() }.getOrNull()
            if (children == null) {
                unreadable++
                continue
            }
            seen += children.size
            val subDirs = ArrayList<Entry>(children.size)
            for (c in children) {
                if (c.isFile && c.name.equals("eboot.bin", ignoreCase = true)) {
                    val childRel = if (rel.isEmpty()) c.name else "$rel/${c.name}"
                    return Outcome(Found(c, childRel, depth + 1, walked),
                                   Stats(walked, seen, unreadable))
                }
                if (c.isDirectory) {
                    val childRel = if (rel.isEmpty()) c.name else "$rel/${c.name}"
                    subDirs.add(Entry(c, depth + 1, childRel))
                }
            }
            if (depth + 1 > maxDepth || walked >= maxDirs) continue
            // Deterministic order: `decrypted` first at this level, then
            // alphabetical. Stable sorts — the last call is the primary key.
            subDirs.sortBy { it.dir.name.lowercase() }
            subDirs.sortByDescending { it.dir.name.equals("decrypted", ignoreCase = true) }
            queue.addAll(subDirs)
        }
        return Outcome(null, Stats(walked, seen, unreadable))
    }

    fun find(root: File, maxDepth: Int = MAX_DEPTH, maxDirs: Int = MAX_DIRS): Found? =
        search(root, maxDepth, maxDirs).found
}
