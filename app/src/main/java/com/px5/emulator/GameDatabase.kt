package com.px5.emulator

import android.content.Context
import androidx.room.Dao
import androidx.room.Database
import androidx.room.Entity
import androidx.room.Insert
import androidx.room.OnConflictStrategy
import androidx.room.PrimaryKey
import androidx.room.Query
import androidx.room.Room
import androidx.room.RoomDatabase
import kotlinx.coroutines.flow.Flow

@Entity(tableName = "games")
data class GameEntity(
    @PrimaryKey val id: String,
    val name: String,
    val path: String,
    val lastPlayed: String = "Never",
    val playTime: String = "0 hours",
    val version: String = "1.00",
    val isFavorite: Boolean = false
)

@Dao
interface GameDao {
    @Query("SELECT * FROM games ORDER BY name ASC")
    fun getAllGames(): Flow<List<GameEntity>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertGame(game: GameEntity)
    
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertGames(games: List<GameEntity>)

    @Query("DELETE FROM games WHERE id = :id")
    suspend fun deleteGameById(id: String)
    
    @Query("UPDATE games SET isFavorite = :isFavorite WHERE id = :id")
    suspend fun updateFavoriteStatus(id: String, isFavorite: Boolean)
}

@Database(entities = [GameEntity::class], version = 1, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {
    abstract fun gameDao(): GameDao

    companion object {
        @Volatile
        private var INSTANCE: AppDatabase? = null

        fun getDatabase(context: Context): AppDatabase {
            return INSTANCE ?: synchronized(this) {
                val instance = Room.databaseBuilder(
                    context.applicationContext,
                    AppDatabase::class.java,
                    "px5_database"
                ).build()
                INSTANCE = instance
                instance
            }
        }
    }
}

class GameRepository(private val gameDao: GameDao) {
    val allGames: Flow<List<GameEntity>> = gameDao.getAllGames()

    suspend fun insert(game: GameEntity) = gameDao.insertGame(game)
    
    suspend fun insertAll(games: List<GameEntity>) = gameDao.insertGames(games)

    suspend fun deleteById(id: String) = gameDao.deleteGameById(id)
    
    suspend fun setFavorite(id: String, isFavorite: Boolean) = gameDao.updateFavoriteStatus(id, isFavorite)
}
