package de.runenkrieg.game.ai

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import de.runenkrieg.game.model.OpponentMode
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import java.security.MessageDigest

/** Independent, learning-free replication of the preselected 10k winner. */
@RunWith(AndroidJUnit4::class)
class TatarusWinnerReplicationInstrumentedTest {
    @Test
    fun replicateFrozenWinnerOnUntouchedSeeds() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val externalRoot = requireNotNull(context.getExternalFilesDir(null))
        val source = File(
            externalRoot,
            "replication_input/tatarus_frozen_winner.json.gz"
        )
        require(source.exists()) { "Missing frozen winner: $source" }

        val stateName = "tatarus_replication_winner.json.gz"
        val internalState = File(context.filesDir, stateName)
        val temporaryState = File(context.filesDir, "$stateName.tmp")
        internalState.delete()
        temporaryState.delete()
        source.copyTo(internalState, overwrite = true)

        val tatarus = TatarusAi(
            context = context,
            stateFileName = stateName,
            modelSeed = WINNER_SEED,
            loadPersistedState = true
        )
        tatarus.setMode(OpponentMode.FROZEN_TATARUS)
        val before = tatarus.summary()
        val evaluation = tatarus.evaluate(
            gamesPerMode = REPLICATION_GAMES,
            modes = listOf(OpponentMode.FROZEN_TATARUS),
            baseSeed = REPLICATION_FIRST_SEED
        )
        val frozen = evaluation.results.single()
        val after = tatarus.summary()

        assertEquals(before, after)
        assertEquals(REPLICATION_GAMES, frozen.games)
        assertEquals(
            REPLICATION_GAMES,
            frozen.wins + frozen.draws + frozen.losses
        )

        val result = JSONObject()
            .put("protocol", "RUNENKRIEG-TATARUS-INDEPENDENT-REPLICATION-1")
            .put("training_seed", WINNER_SEED)
            .put("selection_checkpoint", 10_000)
            .put("replication_seed_first", REPLICATION_FIRST_SEED)
            .put(
                "replication_seed_last",
                REPLICATION_FIRST_SEED + REPLICATION_GAMES - 1
            )
            .put("games", frozen.games)
            .put("wins", frozen.wins)
            .put("draws", frozen.draws)
            .put("losses", frozen.losses)
            .put("game_win_rate", frozen.winRate)
            .put("round_win_rate", frozen.roundWinRate)
            .put("mean_token_swing", frozen.averageTokenSwing)
            .put("decision_ms", frozen.averageDecisionMillis)
            .put("spikes_per_game", frozen.spikesPerGame)
            .put("transmissions_per_game", frozen.transmissionsPerGame)
            .put("energy_cost_per_game", frozen.energyCostPerGame)
            .put("snapshot_sha256", sha256(source))
            .put("learning_disabled", true)
            .put("state_unchanged_after_evaluation", before == after)

        val output = File(externalRoot, "replication_results/replication.json")
        output.parentFile?.mkdirs()
        output.writeText(result.toString(2), Charsets.UTF_8)
        instrumentation.sendStatus(
            REPLICATION_GAMES,
            android.os.Bundle().apply {
                putString(
                    "tatarus_replication",
                    "games=${frozen.games} win=${frozen.winRate}"
                )
            }
        )

        assertTrue(result.getBoolean("learning_disabled"))
        assertTrue(result.getBoolean("state_unchanged_after_evaluation"))
    }

    private fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().buffered().use { stream ->
            val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
            while (true) {
                val read = stream.read(buffer)
                if (read < 0) break
                digest.update(buffer, 0, read)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }

    companion object {
        private const val WINNER_SEED = 20260732
        private const val REPLICATION_FIRST_SEED = 60_000
        private const val REPLICATION_GAMES = 50
    }
}
