package com.px5.emulator

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.media.SoundPool
import android.util.Log

/**
 * Sound manager for PS5-style UI audio (navigation, activation, background music).
 *
 * v1.61 — the whole surface is SESSION-AWARE: while a game session is
 * active (Px5Settings.isGameSessionActive) every UI sound, including the
 * background music, is silent. The PS5 shell ambience belongs to the
 * launcher screens; nothing may play OVER a running game's own audio.
 */
class SoundManager private constructor(private val context: Context) {

    /** True while a game is running — every UI sound stays silent. */
    private val inGameSession: Boolean
        get() = try {
            com.px5.emulator.core.Px5Settings.isGameSessionActive()
        } catch (_: Throwable) { false }

    private var soundPool: SoundPool? = null
    private var navSoundId: Int = 0
    private var actSoundId: Int = 0
    private var bgPlayer: MediaPlayer? = null

    /** Whether sound effects are enabled. */
    var isSoundEnabled: Boolean = true

    /** Whether background music is enabled (setter starts/stops music). */
    var isBgMusicEnabled: Boolean = true
        set(value) {
            field = value
            if (value) {
                startBgMusic()
            } else {
                pauseBgMusic()
            }
        }

    init {
        try {
            val audioAttributes = AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                .build()

            soundPool = SoundPool.Builder()
                .setMaxStreams(5)
                .setAudioAttributes(audioAttributes)
                .build()

            val navFd = context.resources.openRawResourceFd(R.raw.ps5_navigation)
            if (navFd != null) {
                navSoundId = soundPool?.load(navFd, 1) ?: 0
                navFd.close()
            }

            val actFd = context.resources.openRawResourceFd(R.raw.ps5_activation)
            if (actFd != null) {
                actSoundId = soundPool?.load(actFd, 1) ?: 0
                actFd.close()
            }
        } catch (e: Throwable) {
            Log.w("PX5_Sound", "SoundPool initialization disabled or failed: ${e.message}")
            soundPool = null
        }

        initBgMusic()
    }

    private fun initBgMusic() {
        try {
            val bgFd = context.resources.openRawResourceFd(R.raw.ps5_background) ?: return
            val player = MediaPlayer()
            player.setDataSource(bgFd.fileDescriptor, bgFd.startOffset, bgFd.length)
            bgFd.close()
            player.setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build()
            )
            player.isLooping = true
            player.setVolume(0.20f, 0.20f)
            player.setOnErrorListener { mp, what, extra ->
                Log.w("PX5_Sound", "MediaPlayer error: what=$what, extra=$extra")
                try {
                    mp.reset()
                    mp.release()
                } catch (_: Throwable) {}
                bgPlayer = null
                isBgMusicEnabled = false
                true
            }
            player.prepareAsync()
            bgPlayer = player
        } catch (e: Throwable) {
            Log.w("PX5_Sound", "Background music player disabled: ${e.message}")
            bgPlayer = null
        }
    }

    fun playNavigationSound() {
        if (!isSoundEnabled || inGameSession || navSoundId == 0) return
        try {
            soundPool?.play(navSoundId, 0.6f, 0.6f, 1, 0, 1.0f)
        } catch (e: Exception) {
            Log.e("PX5_Sound", "Error playing nav sound: ${e.message}")
        }
    }

    fun playActivationSound() {
        if (!isSoundEnabled || inGameSession || actSoundId == 0) return
        try {
            soundPool?.play(actSoundId, 0.9f, 0.9f, 1, 0, 1.0f)
        } catch (e: Exception) {
            Log.e("PX5_Sound", "Error playing activation sound: ${e.message}")
        }
    }

    fun startBgMusic() {
        // v1.61 — never over a game: onResume fires on every return to the
        // foreground, including mid-session (log share sheet, home gesture).
        // The gate lives HERE, not at the callers, so no screen can forget it.
        if (!isBgMusicEnabled || inGameSession) return
        try {
            if (bgPlayer == null) {
                initBgMusic()
            }
            if (bgPlayer?.isPlaying == false) {
                bgPlayer?.start()
            }
        } catch (e: Exception) {
            Log.e("PX5_Sound", "Error starting bg music: ${e.message}")
        }
    }

    fun pauseBgMusic() {
        try {
            if (bgPlayer?.isPlaying == true) {
                bgPlayer?.pause()
            }
        } catch (e: Exception) {
            Log.e("PX5_Sound", "Error pausing bg music: ${e.message}")
        }
    }

    fun release() {
        try {
            soundPool?.release()
            soundPool = null
            bgPlayer?.stop()
            bgPlayer?.release()
            bgPlayer = null
        } catch (e: Exception) {
            Log.e("PX5_Sound", "Error releasing sound manager: ${e.message}")
        }
    }

    companion object {
        @Volatile
        private var instance: SoundManager? = null

        fun getInstance(context: Context): SoundManager {
            return instance ?: synchronized(this) {
                instance ?: SoundManager(context.applicationContext).also { instance = it }
            }
        }
    }
}
