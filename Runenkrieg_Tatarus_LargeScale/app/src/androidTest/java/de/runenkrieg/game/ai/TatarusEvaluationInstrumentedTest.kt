package de.runenkrieg.game.ai

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import de.runenkrieg.game.model.OpponentMode
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TatarusEvaluationInstrumentedTest {
    @Test
    fun flatJsonRoundTripRestoresTheCompleteLargeScaleState() {
        val original = TatarusNervousSystem(seed = 29)
        val first = DoubleArray(TatarusNervousSystem.INPUT_SIZE) { index ->
            ((index % 8) / 7.0 + 0.35) % 1.0
        }
        val second = DoubleArray(TatarusNervousSystem.INPUT_SIZE) { index ->
            ((index % 8) / 7.0 + 0.65) % 1.0
        }
        original.observe(first, 20, plasticityWrite = true)
        original.applyReward(0.7)
        original.observe(second, 8, plasticityWrite = true)

        val restored = TatarusNervousSystem(seed = 29)
        restored.loadJson(original.toJson())

        assertEquals(original.stateHash(), restored.stateHash())
    }

    @Test
    fun allResearchModesRunFullGamesWithoutChangingThePersistentModel() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        val tatarus = TatarusAi(context)
        val before = tatarus.summary()
        val modes = listOf(
            OpponentMode.PURE_TATARUS,
            OpponentMode.HYBRID,
            OpponentMode.RULE_ONLY,
            OpponentMode.RANDOM,
            OpponentMode.NO_ELIGIBILITY,
            OpponentMode.NO_OPERATOR,
            OpponentMode.NO_ASSEMBLIES
        )

        val evaluation = tatarus.evaluate(
            gamesPerMode = 2,
            modes = modes,
            baseSeed = 20260730
        )

        assertEquals(modes.size, evaluation.results.size)
        assertTrue(evaluation.results.all { it.games == 2 })
        assertTrue(evaluation.results.all {
            it.wins + it.draws + it.losses == it.games
        })
        assertEquals(before, tatarus.summary())
    }
}
