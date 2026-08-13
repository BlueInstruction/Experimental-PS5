package com.px5.emulator

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

class GameViewModel(application: Application) : AndroidViewModel(application) {
    private val repository: GameRepository

    val allGames: StateFlow<List<GameEntity>>

    init {
        val database = AppDatabase.getDatabase(application)
        repository = GameRepository(database.gameDao())

        allGames = repository.allGames.stateIn(
            scope = viewModelScope,
            started = SharingStarted.WhileSubscribed(5000),
            initialValue = emptyList()
        )
        
        viewModelScope.launch {
            allGames.collect { list ->
                if (list.isEmpty()) {
                    seedDefaultPS5Games()
                }
            }
        }
    }

    private suspend fun seedDefaultPS5Games() {
        val defaults = listOf(
            GameEntity(
                id = "ps5_astros_playroom",
                name = "Astro's Playroom",
                path = "/system/app/AstrosPlayroom.elf",
                lastPlayed = "Today",
                playTime = "8 hours",
                version = "1.05",
                isFavorite = true,
                category = "PS5",
                developer = "Asobi Team / PlayStation Studios",
                trophiesUnlocked = 38,
                trophiesTotal = 46,
                bronzeCount = 24,
                silverCount = 10,
                goldCount = 4,
                coverResName = "ps5_custom_cover_bg",
                bannerResName = "ps5_background_all",
                rating = "ESRB E10+",
                sizeGb = "11.2 GB"
            ),
            GameEntity(
                id = "ps5_demons_souls",
                name = "Demon's Souls",
                path = "/sdcard/PX5/Games/DemonsSouls.elf",
                lastPlayed = "Yesterday",
                playTime = "24 hours",
                version = "1.00",
                isFavorite = true,
                category = "PS5",
                developer = "Bluepoint Games / PlayStation Studios",
                trophiesUnlocked = 22,
                trophiesTotal = 37,
                bronzeCount = 14,
                silverCount = 5,
                goldCount = 3,
                coverResName = "ps5_custom_cover_bg",
                bannerResName = "ps5_background_all",
                rating = "ESRB M",
                sizeGb = "66.4 GB"
            ),
            GameEntity(
                id = "ps5_ratchet_clank",
                name = "Ratchet & Clank: Rift Apart",
                path = "/sdcard/PX5/Games/RatchetClank.elf",
                lastPlayed = "3 days ago",
                playTime = "18 hours",
                version = "1.02",
                isFavorite = false,
                category = "PS5",
                developer = "Insomniac Games",
                trophiesUnlocked = 41,
                trophiesTotal = 47,
                bronzeCount = 30,
                silverCount = 8,
                goldCount = 3,
                coverResName = "ps5_custom_cover_bg",
                bannerResName = "ps5_background_all",
                rating = "ESRB E10+",
                sizeGb = "39.8 GB"
            ),
            GameEntity(
                id = "ps5_spiderman2",
                name = "Marvel's Spider-Man 2",
                path = "/sdcard/PX5/Games/SpiderMan2.elf",
                lastPlayed = "1 week ago",
                playTime = "32 hours",
                version = "1.00",
                isFavorite = true,
                category = "PS5",
                developer = "Insomniac Games",
                trophiesUnlocked = 35,
                trophiesTotal = 42,
                bronzeCount = 22,
                silverCount = 10,
                goldCount = 3,
                coverResName = "ps5_custom_cover_bg",
                bannerResName = "ps5_background_all",
                rating = "ESRB T",
                sizeGb = "86.3 GB"
            ),
            GameEntity(
                id = "ps5_god_of_war",
                name = "God of War Ragnarök",
                path = "/sdcard/PX5/Games/GodOfWar.elf",
                lastPlayed = "2 weeks ago",
                playTime = "45 hours",
                version = "1.04",
                isFavorite = false,
                category = "PS5",
                developer = "Santa Monica Studio",
                trophiesUnlocked = 28,
                trophiesTotal = 36,
                bronzeCount = 18,
                silverCount = 7,
                goldCount = 3,
                coverResName = "ps5_custom_cover_bg",
                bannerResName = "ps5_background_all",
                rating = "ESRB M",
                sizeGb = "84.1 GB"
            ),
            GameEntity(
                id = "media_youtube",
                name = "YouTube PS5",
                path = "/system/media/youtube.app",
                lastPlayed = "Today",
                playTime = "120 hours",
                version = "2.10",
                isFavorite = false,
                category = "Media",
                developer = "Google LLC",
                trophiesUnlocked = 0,
                trophiesTotal = 0,
                coverResName = "ps5_custom_cover_bg",
                bannerResName = "ps5_store_background",
                rating = "ESRB Everyone",
                sizeGb = "240 MB"
            ),
            GameEntity(
                id = "media_spotify",
                name = "Spotify Music",
                path = "/system/media/spotify.app",
                lastPlayed = "Yesterday",
                playTime = "85 hours",
                version = "1.80",
                isFavorite = false,
                category = "Media",
                developer = "Spotify AB",
                trophiesUnlocked = 0,
                trophiesTotal = 0,
                coverResName = "ps5_custom_cover_bg",
                bannerResName = "ps5_store_background",
                rating = "ESRB Everyone",
                sizeGb = "180 MB"
            )
        )
        repository.insertAll(defaults)
    }

    fun insert(game: GameEntity) = viewModelScope.launch {
        repository.insert(game)
    }

    fun toggleFavorite(id: String, isFavorite: Boolean) = viewModelScope.launch {
        repository.setFavorite(id, isFavorite)
    }
    
    fun delete(id: String) = viewModelScope.launch {
        repository.deleteById(id)
    }
}
