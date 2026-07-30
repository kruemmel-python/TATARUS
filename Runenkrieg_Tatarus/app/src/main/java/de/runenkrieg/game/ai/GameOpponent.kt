package de.runenkrieg.game.ai

import de.runenkrieg.game.model.AiDecision
import de.runenkrieg.game.model.EvaluationSummary
import de.runenkrieg.game.model.GameState
import de.runenkrieg.game.model.LearningOrigin
import de.runenkrieg.game.model.LearningSummary
import de.runenkrieg.game.model.OpponentMode
import de.runenkrieg.game.model.RuneCard
import de.runenkrieg.game.model.Weather
import de.runenkrieg.game.model.Winner
import kotlin.random.Random

/**
 * Narrow boundary between the game rules and an opponent implementation.
 *
 * The game exposes legal cards and resolved rewards. It never reaches into
 * an opponent's internal memory, which keeps the TATARUS state encapsulated.
 */
interface GameOpponent {
    fun summary(): LearningSummary

    fun choose(
        playerCard: RuneCard,
        aiHand: List<RuneCard>,
        state: GameState,
        weather: Weather,
        random: Random = Random.Default
    ): AiDecision

    fun learn(
        decision: AiDecision,
        reward: Double,
        persist: Boolean = true,
        origin: LearningOrigin = LearningOrigin.REAL_GAME,
        outcome: Winner? = null
    )

    fun train(
        iterations: Int,
        random: Random = Random.Default,
        onProgress: (Int) -> Unit = {}
    )

    fun mode(): OpponentMode

    fun setMode(mode: OpponentMode)

    fun evaluate(
        gamesPerMode: Int = 20,
        modes: List<OpponentMode> =
            OpponentMode.entries.filter { it != OpponentMode.FROZEN_TATARUS },
        baseSeed: Int = 0x54455354,
        onProgress: (Int, Int) -> Unit = { _, _ -> }
    ): EvaluationSummary

    fun reset()
}
