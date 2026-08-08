package com.px5.emulator

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.media.SoundPool
import android.util.Log

class SoundManager private constructor(private val context: Context) {

    private var soundPool: SoundPool? = null
    private var navSoundId: Int = 0
    private var actSoundId: Int = 0
    private var bgPlayer: MediaPlayer? = null
    
    var isSoundEnabled: Boolean = true
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
        val audioAttributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build()

        soundPool = SoundPool.Builder()
            .setMaxStreams(5)
            .setAudioAttributes(audioAttributes)
            .build()

        try {
            navSoundId = soundPool?.load(context, R.raw.ps5_navigation, 1) ?: 0
            actSoundId = soundPool?.load(context, R.raw.ps5_activation, 1) ?: 0
        } catch (e: Exception) {
            Log.e("PX5_Sound", "Failed to load sound effects: ${e.message}")
        }

        initBgMusic()
    }

    private fun initBgMusic() {
        try {
            bgPlayer = MediaPlayer.create(context, R.raw.ps5_background)?.apply {
                isLooping = true
                setVolume(0.20f, 0.20f)
                setOnInfoListener { _, what, extra ->
                    Log.d("PX5_Sound", "MediaPlayer info: what=$what, extra=$extra")
                    false
                }
                setOnErrorListener { mp, what, extra ->
                    Log.w("PX5_Sound", "MediaPlayer error caught: what=$what, extra=$extra. Resetting player state.")
                    try {
                        mp.reset()
                    } catch (e: Throwable) {
                        Log.e("PX5_Sound", "Error resetting MediaPlayer: ${e.message}")
                    }
                    true // Handled error to prevent app crash
                }
            }
        } catch (e: Throwable) {
            Log.w("PX5_Sound", "Failed to create bg music player (handled gracefully for environment without hardware codecs): ${e.message}")
            bgPlayer = null
        }
    }

    fun playNavigationSound() {
        if (!isSoundEnabled || navSoundId == 0) return
        try {
            soundPool?.play(navSoundId, 0.6f, 0.6f, 1, 0, 1.0f)
        } catch (e: Exception) {
            Log.e("PX5_Sound", "Error playing nav sound: ${e.message}")
        }
    }

    fun playActivationSound() {
        if (!isSoundEnabled || actSoundId == 0) return
        try {
            soundPool?.play(actSoundId, 0.9f, 0.9f, 1, 0, 1.0f)
        } catch (e: Exception) {
            Log.e("PX5_Sound", "Error playing activation sound: ${e.message}")
        }
    }

    fun startBgMusic() {
        if (!isBgMusicEnabled) return
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
