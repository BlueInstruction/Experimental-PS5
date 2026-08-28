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

/**
 * GameEntity — a REAL library entry.
 *
 * Every field is backed by something that exists on disk or was actually
 * parsed from a game dump / package file. There are no seeded demo games,
 * no fabricated trophy counts, no invented sizes: values start at zero and
 * are filled in only from real metadata (sce_sys/param.json, PKG header
 * PARAM.SFO, real byte counts from the filesystem).
 *
 * format:     DUMP (decrypted eboot.bin folder) | PKG | ISO | ELF | SELF
 * status:     human-readable honest state ("Ready", "Encrypted payload — "
 *             "decryption pending", "Disc image — extraction pending", ...)
 * coverPath:  absolute path to a cover image that was either copied from
 *             the game folder (sce_sys/icon0.png etc.) or generated from
 *             the title. Empty string = no cover yet.
 */
@Entity(tableName = "games")
data class GameEntity(
    @PrimaryKey val id: String,
    val name: String,
    val titleId: String = "",
    val path: String,
    val isFolder: Boolean = false,
    val format: String = "DUMP",
    val version: String = "",
    val sizeBytes: Long = 0L,
    val coverPath: String = "",
    val status: String = "Ready",
    val lastPlayedMillis: Long = 0L,
    val playTimeSeconds: Long = 0L,
    val installedAtMillis: Long = 0L,
    val isFavorite: Boolean = false
)

@Dao
interface GameDao {
    @Query("SELECT * FROM games ORDER BY lastPlayedMillis DESC, name ASC")
    fun getAllGames(): Flow<List<GameEntity>>

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertGame(game: GameEntity)

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    suspend fun insertGames(games: List<GameEntity>)

    @Query("DELETE FROM games WHERE id = :id")
    suspend fun deleteGameById(id: String)

    @Query("UPDATE games SET isFavorite = :isFavorite WHERE id = :id")
    suspend fun updateFavoriteStatus(id: String, isFavorite: Boolean)

    @Query("UPDATE games SET lastPlayedMillis = :atMillis WHERE id = :id")
    suspend fun updateLastPlayed(id: String, atMillis: Long)

    @Query("UPDATE games SET playTimeSeconds = playTimeSeconds + :seconds WHERE id = :id")
    suspend fun addPlayTime(id: String, seconds: Long)

    @Query("SELECT * FROM games WHERE id = :id LIMIT 1")
    suspend fun getById(id: String): GameEntity?
}

@Database(entities = [GameEntity::class], version = 3, exportSchema = false)
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
                )
                // Schema v2 held seeded demo data; wiping it is the point.
                .fallbackToDestructiveMigration()
                .build()
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

    suspend fun touchPlayed(id: String, atMillis: Long) = gameDao.updateLastPlayed(id, atMillis)

    suspend fun addPlayTime(id: String, seconds: Long) = gameDao.addPlayTime(id, seconds)

    suspend fun getById(id: String) = gameDao.getById(id)
}
