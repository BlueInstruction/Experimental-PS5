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
        
        // Add some default games if empty (handled in UI or initialization)
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
