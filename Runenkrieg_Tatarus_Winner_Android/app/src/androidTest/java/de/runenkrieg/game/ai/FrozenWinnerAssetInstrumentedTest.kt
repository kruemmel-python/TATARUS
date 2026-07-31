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

@RunWith(AndroidJUnit4::class)
class FrozenWinnerAssetInstrumentedTest {
    @Test
    fun packagedWinnerMatchesMetadataAndLoadsWithoutLearning() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val metadata = context.assets.open(METADATA_ASSET).bufferedReader().use {
            JSONObject(it.readText())
        }
        val actualHash = context.assets.open(MODEL_ASSET).use { input ->
            val digest = MessageDigest.getInstance("SHA-256")
            val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
            while (true) {
                val read = input.read(buffer)
                if (read < 0) break
                digest.update(buffer, 0, read)
            }
            digest.digest().joinToString("") { "%02x".format(it) }
        }
        assertEquals(metadata.getString("sha256"), actualHash)
        assertEquals(WINNER_SEED, metadata.getInt("training_seed"))
        assertTrue(metadata.has("independent_replication"))

        val stateName = "instrumented_frozen_winner.json.gz"
        File(context.filesDir, stateName).delete()
        File(context.filesDir, "$stateName.tmp").delete()
        val ai = TatarusAi(
            context = context,
            stateFileName = stateName,
            modelSeed = WINNER_SEED,
            loadPersistedState = true,
            initialAssetName = MODEL_ASSET
        )
        ai.setMode(OpponentMode.FROZEN_TATARUS)
        val before = ai.summary()
        assertEquals(1_024, before.totalNeurons)
        assertEquals(32_768, before.totalSynapses)
        assertEquals(128, before.inputChannels)
        assertTrue(before.trainingObservations >= 10_000)

        ai.evaluate(
            gamesPerMode = 2,
            modes = listOf(OpponentMode.FROZEN_TATARUS),
            baseSeed = 70_000
        )
        assertEquals(before, ai.summary())
    }

    companion object {
        private const val MODEL_ASSET = "tatarus_frozen_winner.snapshot"
        private const val METADATA_ASSET = "winner_metadata.json"
        private const val WINNER_SEED = 20260732
    }
}
