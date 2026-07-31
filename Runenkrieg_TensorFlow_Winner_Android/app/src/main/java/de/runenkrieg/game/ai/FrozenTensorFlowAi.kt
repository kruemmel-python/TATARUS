package de.runenkrieg.game.ai

import android.content.Context
import de.runenkrieg.game.model.AiDecision
import de.runenkrieg.game.model.CardType
import de.runenkrieg.game.model.Element
import de.runenkrieg.game.model.EvaluationSummary
import de.runenkrieg.game.model.GameState
import de.runenkrieg.game.model.LearningOrigin
import de.runenkrieg.game.model.LearningSummary
import de.runenkrieg.game.model.Mechanic
import de.runenkrieg.game.model.OpponentMode
import de.runenkrieg.game.model.RuleBook
import de.runenkrieg.game.model.RuneCard
import de.runenkrieg.game.model.Weather
import de.runenkrieg.game.model.Winner
import org.json.JSONObject
import org.tensorflow.lite.Interpreter
import java.io.FileInputStream
import java.nio.MappedByteBuffer
import java.nio.channels.FileChannel
import java.security.MessageDigest
import java.util.ArrayDeque
import kotlin.math.max
import kotlin.random.Random

/**
 * Read-only mobile opponent selected by RUNENKRIEG-TF-MULTISEED-1.
 *
 * The model, training seed and SHA-256 digest live in winner_metadata.json.
 * Android performs inference only. It neither updates model parameters nor
 * feeds played rounds back into the frozen checkpoint.
 */
class FrozenTensorFlowAi(context: Context) : GameOpponent, AutoCloseable {
    private data class Option(
        val card: RuneCard,
        val consumedIds: Set<String>
    )

    private val applicationContext = context.applicationContext
    private val metadata = JSONObject(
        applicationContext.assets.open(METADATA_ASSET)
            .bufferedReader()
            .use { it.readText() }
    )
    private val agentName = metadata.getString("agent")
    private val trainingRounds = metadata.getInt("selection_checkpoint")
    private val modelBytes = metadata.getLong("bytes")
    private val historyLength = metadata.optInt("history_length", 1).coerceAtLeast(1)
    private val modelBuffer = mapAsset(applicationContext, MODEL_ASSET)
    private val actualSha256 = sha256(modelBuffer)
    private val interpreter = Interpreter(
        modelBuffer.also {
            require(
                actualSha256.equals(metadata.getString("sha256"), ignoreCase = true)
            ) {
                "Die SHA-256-Prüfsumme des eingefrorenen Modells stimmt nicht."
            }
        },
        Interpreter.Options().apply { setNumThreads(4) }
    )
    private val history = ArrayDeque<FloatArray>()

    private var decisions = 0L
    private var decisionNanos = 0L
    private var realRounds = 0L
    private var realWins = 0L
    private var realDraws = 0L
    private var realLosses = 0L
    private var rewardSum = 0.0
    private var pendingHistory: FloatArray? = null

    @Synchronized
    override fun summary(): LearningSummary = LearningSummary(
        observations = decisions,
        contexts = decisions,
        actions = 0,
        averageReward = if (realRounds == 0L) 0.0 else rewardSum / realRounds,
        explorationRate = 0.0,
        trainingRuns = 0,
        mode = OpponentMode.FROZEN_TATARUS,
        realObservations = realRounds,
        realAverageReward = if (realRounds == 0L) 0.0 else rewardSum / realRounds,
        realWins = realWins,
        realDraws = realDraws,
        realLosses = realLosses,
        trainingObservations = trainingRounds.toLong(),
        trainingAverageReward =
            metadata.optJSONObject("independent_replication")
                ?.optDouble("mean_reward", 0.0)
                ?: 0.0,
        stateBytesEstimate = modelBytes,
        measuredDecisions = decisions,
        averageDecisionMillis =
            if (decisions == 0L) 0.0
            else decisionNanos.toDouble() / decisions / 1_000_000.0,
        inputChannels = INPUT_SIZE
    )

    @Synchronized
    override fun choose(
        playerCard: RuneCard,
        aiHand: List<RuneCard>,
        state: GameState,
        weather: Weather,
        random: Random
    ): AiDecision {
        require(aiHand.isNotEmpty()) { "Der eingefrorene Gegner benötigt eine Karte." }
        if (state.history.isEmpty()) history.clear()
        val started = System.nanoTime()
        val options = createOptions(aiHand)
        val features = options.map {
            encode(state, playerCard, it, aiHand, weather)
        }
        val scores = infer(features)
        val bestIndex = scores.indices.maxBy { scores[it] }
        val chosen = options[bestIndex]
        pendingHistory = features[bestIndex].copyOf()
        decisions += 1
        decisionNanos += (System.nanoTime() - started).coerceAtLeast(0L)
        return AiDecision(
            card = chosen.card,
            consumedCardIds = chosen.consumedIds,
            contextKey =
                "${state.round}|${playerCard.learningKey}|${weather.name}",
            actionKey = chosen.card.learningKey,
            learnedScore = scores[bestIndex].toDouble(),
            heuristicScore = 0.0,
            wasExploration = false
        )
    }

    @Synchronized
    override fun learn(
        decision: AiDecision,
        reward: Double,
        persist: Boolean,
        origin: LearningOrigin,
        outcome: Winner?
    ) {
        pendingHistory?.let { selected ->
            if (historyLength > 1) {
                while (history.size >= historyLength - 1) history.removeFirst()
                history.addLast(selected)
            }
        }
        pendingHistory = null
        if (origin != LearningOrigin.REAL_GAME) return
        realRounds += 1
        rewardSum += reward.coerceIn(0.0, 1.0)
        when (outcome) {
            Winner.AI -> realWins += 1
            Winner.DRAW -> realDraws += 1
            Winner.PLAYER -> realLosses += 1
            null -> Unit
        }
    }

    override fun train(
        iterations: Int,
        random: Random,
        onProgress: (Int) -> Unit
    ) {
        onProgress(iterations.coerceAtLeast(0))
    }

    override fun mode(): OpponentMode = OpponentMode.FROZEN_TATARUS

    override fun setMode(mode: OpponentMode) = Unit

    override fun evaluate(
        gamesPerMode: Int,
        modes: List<OpponentMode>,
        baseSeed: Int,
        onProgress: (Int, Int) -> Unit
    ): EvaluationSummary {
        onProgress(0, 0)
        return EvaluationSummary()
    }

    @Synchronized
    override fun reset() {
        history.clear()
        pendingHistory = null
        decisions = 0
        decisionNanos = 0
        realRounds = 0
        realWins = 0
        realDraws = 0
        realLosses = 0
        rewardSum = 0.0
    }

    override fun close() {
        interpreter.close()
    }

    private fun infer(features: List<FloatArray>): FloatArray {
        val output = Array(features.size) { FloatArray(1) }
        if (historyLength == 1) {
            interpreter.resizeInput(0, intArrayOf(features.size, INPUT_SIZE))
            interpreter.allocateTensors()
            interpreter.run(features.toTypedArray(), output)
        } else {
            val sequences = Array(features.size) { candidateIndex ->
                sequenceFor(features[candidateIndex])
            }
            interpreter.resizeInput(
                0,
                intArrayOf(features.size, historyLength, INPUT_SIZE)
            )
            interpreter.allocateTensors()
            interpreter.run(sequences, output)
        }
        return FloatArray(features.size) { output[it][0] }
    }

    private fun sequenceFor(candidate: FloatArray): Array<FloatArray> {
        val sequence = Array(historyLength) { FloatArray(INPUT_SIZE) }
        val prefix = historyLength - 1 - history.size
        history.forEachIndexed { index, item ->
            sequence[prefix + index] = item.copyOf()
        }
        sequence[historyLength - 1] = candidate
        return sequence
    }

    private fun createOptions(hand: List<RuneCard>): List<Option> {
        val options = hand.map { Option(it, setOf(it.id)) }.toMutableList()
        val fusionCards = hand.filter { Mechanic.FUSION in it.mechanics }
        for (firstIndex in 0 until fusionCards.lastIndex) {
            for (secondIndex in firstIndex + 1 until fusionCards.size) {
                val first = fusionCards[firstIndex]
                val second = fusionCards[secondIndex]
                options += Option(
                    RuleBook.fuse(first, second),
                    setOf(first.id, second.id)
                )
            }
        }
        return options
    }

    private fun encode(
        state: GameState,
        playerCard: RuneCard,
        option: Option,
        hand: List<RuneCard>,
        weather: Weather
    ): FloatArray {
        val card = option.card
        val input = FloatArray(INPUT_SIZE)
        input[card.element.ordinal] = 1f
        input[10] = card.ability.power / 13f
        input[11] = card.type.ordinal / 4f
        input[12] = if (card.fused) 1f else 0f
        input[13] = card.mechanics.size / 7f
        input[14] = 0f
        input[15] = 0f
        input[16] = (state.aiTokens / 12f).coerceIn(0f, 1f)
        input[17] = (state.playerTokens / 12f).coerceIn(0f, 1f)
        input[18] = if (state.aiHero.element == card.element) 1f else 0f
        Mechanic.entries.forEachIndexed { index, mechanic ->
            input[19 + index] = if (mechanic in card.mechanics) 1f else 0f
        }
        input[26] = if (card.type == CardType.ARTIFACT) 1f else 0f
        input[27] = if (card.type == CardType.SUMMON) 1f else 0f
        input[28] = 0f
        input[29] = option.consumedIds.size / 2f
        input[30] =
            state.history.takeLast(4).count { it.aiCard.element == card.element } / 4f
        input[31] = 1f
        input[32 + card.ability.ordinal] = 1f
        input[46 + card.type.ordinal] = 1f
        Mechanic.entries.forEachIndexed { index, mechanic ->
            input[51 + index] = if (mechanic in card.mechanics) 1f else 0f
        }
        input[58 + playerCard.element.ordinal] = 1f
        input[68 + playerCard.element.ordinal] = card.ability.power / 13f
        input[78 + card.element.ordinal] = (weather.ordinal + 1) / 3f
        Element.entries.forEachIndexed { index, element ->
            input[88 + index] =
                hand.count { it.element == element } / max(1, hand.size).toFloat()
            input[98 + index] =
                state.history.takeLast(6).count { it.aiCard.element == element } / 6f
            input[108 + index] =
                state.history.takeLast(6).count { it.playerCard.element == element } / 6f
        }
        val power = card.ability.power / 13f
        input[118] = power * power
        input[119] =
            ((state.aiTokens - state.playerTokens + 10) / 20f).coerceIn(0f, 1f)
        input[120] = if (state.aiTokens <= 2) 1f else 0f
        input[121] = if (card.ability.power >= 12) 1f else 0f
        input[122] = (state.round / 100f).coerceIn(0f, 1f)
        input[123] =
            state.history.takeLast(6).count {
                it.aiCard.learningKey == card.learningKey
            } / 6f
        input[124] =
            hand.count { it.element == card.element } / max(1, hand.size).toFloat()
        input[125] =
            ((hand.size - option.consumedIds.size).coerceAtLeast(0) / 4f)
                .coerceIn(0f, 1f)
        input[126] =
            state.history.takeLast(4).count { it.winner == Winner.AI } / 4f
        input[127] = 1f
        return input
    }

    private fun mapAsset(context: Context, assetName: String): MappedByteBuffer {
        return context.assets.openFd(assetName).use { descriptor ->
            FileInputStream(descriptor.fileDescriptor).channel.use { channel ->
                channel.map(
                    FileChannel.MapMode.READ_ONLY,
                    descriptor.startOffset,
                    descriptor.declaredLength
                )
            }
        }
    }

    private fun sha256(buffer: MappedByteBuffer): String {
        val digest = MessageDigest.getInstance("SHA-256")
        val duplicate = buffer.asReadOnlyBuffer()
        duplicate.position(0)
        val chunk = ByteArray(64 * 1024)
        while (duplicate.hasRemaining()) {
            val length = minOf(chunk.size, duplicate.remaining())
            duplicate.get(chunk, 0, length)
            digest.update(chunk, 0, length)
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }

    companion object {
        private const val INPUT_SIZE = 128
        private const val MODEL_ASSET = "runenkrieg_frozen_winner.tflite"
        private const val METADATA_ASSET = "winner_metadata.json"
    }
}
