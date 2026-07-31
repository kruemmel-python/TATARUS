package de.runenkrieg.game.ai

import android.os.Debug
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import de.runenkrieg.game.model.OpponentMode
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import java.io.File
import kotlin.random.Random

/**
 * Device-native TATARUS learning curve.
 *
 * One instrumentation invocation owns exactly one independent model seed.
 * Training and holdout evaluation never share randomness, and evaluate()
 * restores the complete model checkpoint after every holdout.
 */
@RunWith(AndroidJUnit4::class)
class TatarusTenKBenchmarkInstrumentedTest {
    @Test
    fun runRequestedTrainingSeed() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val arguments = InstrumentationRegistry.getArguments()
        val context = instrumentation.targetContext
        val seed = arguments.getString("trainingSeed")?.toInt()
            ?: error("Missing instrumentation argument: trainingSeed")
        val quick = arguments.getString("quick")?.toBooleanStrictOrNull() ?: false
        val resume = arguments.getString("resume")?.toBooleanStrictOrNull() ?: false
        val requestedMaximum = arguments.getString("maxCheckpoint")?.toIntOrNull()
        val allCheckpoints = if (quick) intArrayOf(25, 50) else CHECKPOINTS
        val checkpoints = if (requestedMaximum == null) {
            allCheckpoints
        } else {
            allCheckpoints.filter { it <= requestedMaximum }.toIntArray()
        }
        require(checkpoints.isNotEmpty()) { "maxCheckpoint precedes the first checkpoint" }
        val evaluationGames = if (quick) 2 else HOLDOUT_GAMES

        val outputRoot = File(
            context.getExternalFilesDir(null),
            if (quick) "tatarus_benchmark_quick" else "tatarus_benchmark_full"
        )
        val seedDirectory = File(outputRoot, "seed_$seed")
        require(seedDirectory.canonicalPath.startsWith(outputRoot.canonicalPath))
        val stateFileName =
            if (quick) "benchmark_quick_seed_$seed.json.gz"
            else "benchmark_full_seed_$seed.json.gz"
        if (!resume) {
            seedDirectory.deleteRecursively()
            File(context.filesDir, stateFileName).delete()
            File(context.filesDir, "$stateFileName.tmp").delete()
        }
        seedDirectory.mkdirs()
        val curveFile = File(seedDirectory, "learning_curve.json")
        val existingRows = if (resume && curveFile.exists()) {
            JSONArray(curveFile.readText(Charsets.UTF_8))
        } else {
            JSONArray()
        }
        val completedBeforeStart = if (existingRows.length() == 0) {
            0
        } else {
            existingRows.getJSONObject(existingRows.length() - 1)
                .getInt("environment_rounds")
        }
        val internalState = File(context.filesDir, stateFileName)
        if (resume && !internalState.exists() && completedBeforeStart > 0) {
            val externalSnapshot = File(
                seedDirectory,
                "round_$completedBeforeStart/" +
                    "tatarus_seed_${seed}_round_$completedBeforeStart.json.gz"
            )
            require(externalSnapshot.exists()) {
                "Resume needs internal state or pulled-back snapshot: $externalSnapshot"
            }
            externalSnapshot.copyTo(internalState, overwrite = true)
        }
        val tatarus = TatarusAi(
            context = context,
            stateFileName = stateFileName,
            modelSeed = seed,
            loadPersistedState = resume
        )
        if (!resume) {
            tatarus.reset()
            tatarus.setMode(OpponentMode.PURE_TATARUS)
        }
        val rows = existingRows
        var trained = completedBeforeStart
        var trainingCpuNanos = if (rows.length() == 0) {
            0L
        } else {
            (rows.getJSONObject(rows.length() - 1)
                .getDouble("training_cpu_seconds") * 1_000_000_000.0).toLong()
        }
        var trainingWallNanos = if (rows.length() == 0) {
            0L
        } else {
            (rows.getJSONObject(rows.length() - 1)
                .getDouble("training_wall_seconds") * 1_000_000_000.0).toLong()
        }

        checkpoints.filter { it > trained }.forEach { checkpoint ->
            val delta = checkpoint - trained
            val trainingRandom = Random(
                seed xor TRAINING_RANDOM_MASK xor trained
            )
            val cpuStarted = Debug.threadCpuTimeNanos()
            val wallStarted = System.nanoTime()
            tatarus.train(delta, trainingRandom)
            trainingCpuNanos +=
                (Debug.threadCpuTimeNanos() - cpuStarted).coerceAtLeast(0L)
            trainingWallNanos +=
                (System.nanoTime() - wallStarted).coerceAtLeast(0L)
            trained = checkpoint

            val evaluation = tatarus.evaluate(
                gamesPerMode = evaluationGames,
                modes = listOf(OpponentMode.FROZEN_TATARUS),
                baseSeed = HOLDOUT_FIRST_SEED
            )
            val frozen = evaluation.results.single()
            val snapshot = File(
                seedDirectory,
                "round_$checkpoint/tatarus_seed_${seed}_round_$checkpoint.json.gz"
            )
            tatarus.saveSnapshot(snapshot)
            val summary = tatarus.summary()
            val row = JSONObject()
                .put("agent", "tatarus_large_scale")
                .put("seed", seed)
                .put("environment_rounds", checkpoint)
                .put("evaluation_games", evaluationGames)
                .put("holdout_seed_first", HOLDOUT_FIRST_SEED)
                .put("holdout_seed_last", HOLDOUT_FIRST_SEED + evaluationGames - 1)
                .put("game_win_rate", frozen.winRate)
                .put("game_draw_rate", frozen.draws.toDouble() / evaluationGames)
                .put("round_win_rate", frozen.roundWinRate)
                .put(
                    "round_draw_rate",
                    frozen.roundDraws.toDouble() /
                        (frozen.roundWins + frozen.roundDraws + frozen.roundLosses)
                            .coerceAtLeast(1)
                )
                .put("mean_token_swing", frozen.averageTokenSwing)
                .put("decision_ms", frozen.averageDecisionMillis)
                .put("spikes_per_game", frozen.spikesPerGame)
                .put("transmissions_per_game", frozen.transmissionsPerGame)
                .put("energy_cost_per_game", frozen.energyCostPerGame)
                .put("training_cpu_seconds", trainingCpuNanos / 1_000_000_000.0)
                .put("training_wall_seconds", trainingWallNanos / 1_000_000_000.0)
                .put("parameter_bytes", snapshot.length())
                .put("neural_state_bytes_estimate", summary.stateBytesEstimate)
                .put("model_seed", seed)
                .put("snapshot", snapshot.name)
            File(snapshot.parentFile, "metrics.json")
                .writeText(row.toString(2), Charsets.UTF_8)
            rows.put(row)
            curveFile
                .writeText(rows.toString(2), Charsets.UTF_8)
            instrumentation.sendStatus(
                checkpoint,
                android.os.Bundle().apply {
                    putString(
                        "tatarus_checkpoint",
                        "seed=$seed rounds=$checkpoint win=${frozen.winRate}"
                    )
                }
            )
        }

        val manifest = JSONObject()
            .put("protocol", PROTOCOL)
            .put("training_seed", seed)
            .put("model_seed", seed)
            .put("checkpoints", JSONArray(checkpoints.toList()))
            .put("holdout_seeds", JSONArray(listOf(
                HOLDOUT_FIRST_SEED,
                HOLDOUT_FIRST_SEED + evaluationGames - 1
            )))
            .put("learning_disabled_during_holdout", true)
            .put("opponent_policy_training", "mixed")
            .put("opponent_policy_holdout", "random")
            .put("segment_randomization", "seed xor 0x0A11CE xor previous_checkpoint")
            .put("completed", trained == allCheckpoints.last())
        File(seedDirectory, "manifest.json")
            .writeText(manifest.toString(2), Charsets.UTF_8)

        assertEquals(checkpoints.last(), trained)
        assertTrue(rows.length() >= checkpoints.size)
        assertTrue(tatarus.persistentStateFile().exists())
    }

    companion object {
        private const val PROTOCOL = "RUNENKRIEG-TATARUS-MULTISEED-1"
        private val CHECKPOINTS = intArrayOf(250, 500, 1_000, 2_000, 5_000, 10_000)
        private const val HOLDOUT_GAMES = 20
        private const val HOLDOUT_FIRST_SEED = 30_000
        private const val TRAINING_RANDOM_MASK = 0x0A11CE
    }
}
