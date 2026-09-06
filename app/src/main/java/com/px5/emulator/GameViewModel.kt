package com.px5.emulator

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

/**
 * GameViewModel — exposes the REAL game library.
 *
 * The previous version seeded a fake library (God of War Ragnarök,
 * Astro's Playroom, ...) with invented trophy counts and play times the
 * moment the real list was empty — so deleting every game brought the
 * fiction back. That seeding is gone for good: an empty database shows
 * an honest "no games yet" state until the user imports one.
 */
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
    }

    /**
     * Inserts a game into the database.
     * @param game Game entity to insert
     */
    fun insert(game: GameEntity) = viewModelScope.launch {
        repository.insert(game)
    }

    /**
     * Inserts multiple games into the database.
     * @param games List of game entities to insert
     */
    fun insertAll(games: List<GameEntity>) = viewModelScope.launch {
        repository.insertAll(games)
    }

    /**
     * Toggles a game's favorite status.
     * @param id Game ID
     * @param isFavorite true to mark as favorite, false otherwise
     */
    fun toggleFavorite(id: String, isFavorite: Boolean) = viewModelScope.launch {
        repository.setFavorite(id, isFavorite)
    }

    /**
     * Deletes a game from the database.
     * @param id Game ID to delete
     */
    fun delete(id: String) = viewModelScope.launch {
        repository.deleteById(id)
    }

    /** Mark a game as played now (called when the emulation screen opens). */
    fun touchPlayed(id: String) = viewModelScope.launch {
        repository.touchPlayed(id, System.currentTimeMillis())
    }

    /** Accumulate real wall-clock session seconds (called from EmuScreen). */
    fun addPlayTime(id: String, seconds: Long) = viewModelScope.launch {
        if (seconds > 0) repository.addPlayTime(id, seconds)
    }
}
