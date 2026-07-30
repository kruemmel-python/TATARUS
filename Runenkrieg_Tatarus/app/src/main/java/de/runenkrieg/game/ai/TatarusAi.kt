package de.runenkrieg.game.ai

import android.content.Context
import de.runenkrieg.game.engine.GameEngine
import de.runenkrieg.game.model.Ability
import de.runenkrieg.game.model.AiDecision
import de.runenkrieg.game.model.CardType
import de.runenkrieg.game.model.Element
import de.runenkrieg.game.model.EvaluationModeResult
import de.runenkrieg.game.model.EvaluationSummary
import de.runenkrieg.game.model.GamePhase
import de.runenkrieg.game.model.GameState
import de.runenkrieg.game.model.Hero
import de.runenkrieg.game.model.LearningOrigin
import de.runenkrieg.game.model.LearningSummary
import de.runenkrieg.game.model.Mechanic
import de.runenkrieg.game.model.OpponentMode
import de.runenkrieg.game.model.RuleBook
import de.runenkrieg.game.model.RuneCard
import de.runenkrieg.game.model.Weather
import de.runenkrieg.game.model.Winner
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.atomic.AtomicLong
import kotlin.math.max
import kotlin.math.sqrt
import kotlin.math.tanh
import kotlin.random.Random

/**
 * TATARUS – A Persistent Synthetic Nervous System as the Runenkrieg opponent.
 *
 * Every observed round changes a continuously maintained recurrent nervous
 * state. Legal cards are evaluated through short counterfactual neural
 * rollouts; only the selected rollout is committed. The resolved consequence
 * then modifies local synaptic eligibility and a bounded action readout.
 */
class TatarusAi(context: Context) : GameOpponent {
    private data class ActionStats(var visits: Long = 0, var reward: Double = 0.0)

    private data class Option(
        val card: RuneCard,
        val consumedIds: Set<String>,
        val ruleScore: Double
    )

    private data class Assessment(
        val option: Option,
        val policyFeatures: DoubleArray,
        val learnedScore: Double,
        val totalScore: Double
    )

    private data class ModelSnapshot(
        val nervousSystem: TatarusNervousSystem.DynamicSnapshot,
        val readouts: Map<String, DoubleArray>,
        val actionStats: Map<String, ActionStats>,
        val contextHashes: Set<Long>,
        val pendingFeatures: Map<String, DoubleArray>,
        val observations: Long,
        val totalReward: Double,
        val realObservations: Long,
        val realReward: Double,
        val realWins: Long,
        val realDraws: Long,
        val realLosses: Long,
        val trainingObservations: Long,
        val trainingReward: Double,
        val trainingRuns: Int,
        val mode: OpponentMode
    )

    private val preferences =
        context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    private var nervousSystem = TatarusNervousSystem()
    private val readouts = linkedMapOf<String, DoubleArray>()
    private val actionStats = linkedMapOf<String, ActionStats>()
    private val seenContextHashes = linkedSetOf<Long>()
    private val pendingFeatures = linkedMapOf<String, DoubleArray>()
    private val idCounter = AtomicLong()

    private var observations = 0L
    private var totalReward = 0.0
    private var realObservations = 0L
    private var realReward = 0.0
    private var realWins = 0L
    private var realDraws = 0L
    private var realLosses = 0L
    private var trainingObservations = 0L
    private var trainingReward = 0.0
    private var trainingRuns = 0
    private var currentMode = OpponentMode.PURE_TATARUS
    private var evaluationActive = false

    init {
        load()
    }

    @Synchronized
    override fun summary(): LearningSummary {
        val metrics = nervousSystem.metrics()
        return LearningSummary(
            observations = observations,
            contexts = seenContextHashes.size.toLong(),
            actions = actionStats.size,
            averageReward = if (observations == 0L) 0.0 else totalReward / observations,
            explorationRate = explorationRate(),
            trainingRuns = trainingRuns,
            mode = currentMode,
            realObservations = realObservations,
            realAverageReward =
                if (realObservations == 0L) 0.0 else realReward / realObservations,
            realWins = realWins,
            realDraws = realDraws,
            realLosses = realLosses,
            trainingObservations = trainingObservations,
            trainingAverageReward =
                if (trainingObservations == 0L) 0.0
                else trainingReward / trainingObservations,
            neuralSteps = metrics.steps,
            neuralSpikes = metrics.spikes,
            spikeRateHz = if (metrics.steps == 0L) {
                0.0
            } else {
                metrics.spikes.toDouble() / (metrics.steps * NEURON_COUNT_FOR_RATE) * 1000.0
            },
            transmissions = metrics.transmissions,
            totalSynapses = metrics.synapses,
            recentlyActiveSynapses = metrics.recentlyActiveSynapses,
            saturatedWeightFraction = metrics.saturatedWeightFraction,
            activeAssemblies = metrics.assemblies,
            assemblyEntropy = metrics.assemblyEntropy,
            assemblySeparation = metrics.assemblySeparation,
            assemblyReactivations = metrics.assemblyReactivations,
            meanEnergy = metrics.meanEnergy,
            minimumEnergy = metrics.minimumEnergy,
            energyP10 = metrics.energyP10,
            energyCostPerObservation = if (observations == 0L) {
                0.0
            } else {
                (metrics.spikes * SPIKE_COST +
                    metrics.transmissions * TRANSMISSION_COST) / observations
            },
            meanEligibility = metrics.meanEligibility,
            meanAbsoluteEligibility = metrics.meanAbsoluteEligibility,
            eligibilityStdDev = metrics.eligibilityStdDev,
            maximumAbsoluteEligibility = metrics.maximumAbsoluteEligibility,
            activeEligibilityFraction = metrics.activeEligibilityFraction,
            saturatedEligibilityFraction = metrics.saturatedEligibilityFraction,
            positiveEligibilityFraction = metrics.positiveEligibilityFraction,
            negativeEligibilityFraction = metrics.negativeEligibilityFraction
        )
    }

    @Synchronized
    override fun choose(
        playerCard: RuneCard,
        aiHand: List<RuneCard>,
        state: GameState,
        weather: Weather,
        random: Random
    ): AiDecision {
        require(aiHand.isNotEmpty()) { "TATARUS benötigt mindestens eine Karte." }

        val mode = currentMode
        val contextKey = createContextKey(playerCard, weather, state)
        seenContextHashes += stableHash64(contextKey)
        val options = createOptions(playerCard, aiHand, state, weather)

        if (mode == OpponentMode.RANDOM || mode == OpponentMode.RULE_ONLY) {
            val chosen = if (mode == OpponentMode.RANDOM) {
                options.random(random)
            } else {
                options.maxBy { it.ruleScore }
            }
            return AiDecision(
                card = chosen.card,
                consumedCardIds = chosen.consumedIds,
                contextKey = contextKey,
                actionKey = chosen.card.learningKey,
                learnedScore = 0.5,
                heuristicScore = chosen.ruleScore,
                wasExploration = mode == OpponentMode.RANDOM
            )
        }

        val useEligibility = mode != OpponentMode.NO_ELIGIBILITY
        val useGeneratedOperator = mode != OpponentMode.NO_OPERATOR
        val exposeAssemblies = mode != OpponentMode.NO_ASSEMBLIES
        val writePlasticity =
            mode.trainable && mode != OpponentMode.FROZEN_TATARUS && !evaluationActive
        val includeDerivedRules = mode == OpponentMode.HYBRID
        val contextInput = encodeContext(playerCard, weather, state)
        nervousSystem.observe(
            contextInput,
            duration = CONTEXT_STEPS,
            plasticityWrite = writePlasticity,
            updateAssemblies = writePlasticity,
            useEligibility = useEligibility,
            useGeneratedOperator = useGeneratedOperator,
            exposeAssemblies = exposeAssemblies
        )

        val baseline = nervousSystem.checkpoint()
        val assessments = options.map { option ->
            nervousSystem.restore(baseline)
            val candidate = encodeCandidate(
                option = option,
                playerCard = playerCard,
                state = state,
                weather = weather,
                includeDerivedRules = includeDerivedRules
            )
            val bridge = nervousSystem.observe(
                candidate,
                duration = CANDIDATE_STEPS,
                plasticityWrite = false,
                updateAssemblies = false,
                useEligibility = useEligibility,
                useGeneratedOperator = useGeneratedOperator,
                exposeAssemblies = exposeAssemblies
            )
            val features = policyFeatures(bridge, candidate)
            val weights = readouts.getOrPut(option.card.learningKey) {
                initialReadout(option.card.learningKey)
            }
            val neuralScore = tanh(dot(weights, features))
            val stats = actionStats[option.card.learningKey]
            val empirical = if (stats == null || stats.visits == 0L) {
                0.0
            } else {
                (stats.reward / stats.visits) * 2.0 - 1.0
            }
            val rulePrior = tanh(option.ruleScore / 18.0)
            Assessment(
                option = option,
                policyFeatures = features,
                learnedScore = (neuralScore + 1.0) * 0.5,
                totalScore = if (mode == OpponentMode.HYBRID) {
                    NEURAL_WEIGHT * neuralScore +
                        RULE_PRIOR_WEIGHT * rulePrior +
                        EMPIRICAL_WEIGHT * empirical
                } else {
                    neuralScore
                }
            )
        }
        nervousSystem.restore(baseline)

        val explore =
            !evaluationActive &&
                mode.trainable &&
                random.nextDouble() < explorationRate()
        val chosen = if (explore) {
            val leastVisited = assessments.minOf {
                actionStats[it.option.card.learningKey]?.visits ?: 0L
            }
            assessments.filter {
                (actionStats[it.option.card.learningKey]?.visits ?: 0L) == leastVisited
            }.random(random)
        } else {
            assessments.maxBy { it.totalScore }
        }

        val committedCandidate =
            encodeCandidate(
                option = chosen.option,
                playerCard = playerCard,
                state = state,
                weather = weather,
                includeDerivedRules = includeDerivedRules
            )
        val committedBridge = nervousSystem.observe(
            committedCandidate,
            duration = CANDIDATE_STEPS,
            plasticityWrite = writePlasticity,
            updateAssemblies = writePlasticity,
            useEligibility = useEligibility,
            useGeneratedOperator = useGeneratedOperator,
            exposeAssemblies = exposeAssemblies
        )
        val committedFeatures = policyFeatures(committedBridge, committedCandidate)
        if (writePlasticity) {
            val pendingKey = pendingKey(contextKey, chosen.option.card.learningKey)
            pendingFeatures[pendingKey] = committedFeatures
        }

        return AiDecision(
            card = chosen.option.card,
            consumedCardIds = chosen.option.consumedIds,
            contextKey = contextKey,
            actionKey = chosen.option.card.learningKey,
            learnedScore = chosen.learnedScore,
            heuristicScore = chosen.option.ruleScore,
            wasExploration = explore
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
        if (evaluationActive || origin == LearningOrigin.EVALUATION) return
        val boundedReward = reward.coerceIn(0.0, 1.0)
        val centeredReward = boundedReward * 2.0 - 1.0

        when (origin) {
            LearningOrigin.REAL_GAME -> {
                realObservations += 1
                realReward += boundedReward
                when (outcome) {
                    Winner.AI -> realWins += 1
                    Winner.DRAW -> realDraws += 1
                    Winner.PLAYER -> realLosses += 1
                    null -> Unit
                }
            }
            LearningOrigin.SELF_TRAINING -> {
                trainingObservations += 1
                trainingReward += boundedReward
            }
            LearningOrigin.EVALUATION -> Unit
        }

        val mode = currentMode
        if (!mode.trainable) {
            if (persist) save()
            return
        }

        val features =
            pendingFeatures.remove(pendingKey(decision.contextKey, decision.actionKey))
                ?: DoubleArray(POLICY_FEATURES).also { it[it.lastIndex] = 1.0 }
        val weights = readouts.getOrPut(decision.actionKey) {
            initialReadout(decision.actionKey)
        }
        val prediction = tanh(dot(weights, features))
        val error = centeredReward - prediction
        weights.indices.forEach { index ->
            weights[index] =
                (weights[index] + POLICY_LEARNING_RATE * error * features[index])
                    .coerceIn(-READOUT_BOUND, READOUT_BOUND)
        }

        val stats = actionStats.getOrPut(decision.actionKey) { ActionStats() }
        stats.visits += 1
        stats.reward += boundedReward
        observations += 1
        totalReward += boundedReward

        val useEligibility = mode != OpponentMode.NO_ELIGIBILITY
        val useGeneratedOperator = mode != OpponentMode.NO_OPERATOR
        val exposeAssemblies = mode != OpponentMode.NO_ASSEMBLIES
        nervousSystem.applyReward(centeredReward, useEligibility = useEligibility)
        val consequence = DoubleArray(TatarusNervousSystem.INPUT_SIZE)
        consequence[0] = boundedReward
        consequence[1] = 1.0 - boundedReward
        consequence[30] = if (centeredReward >= 0.0) centeredReward else 0.0
        consequence[31] = if (centeredReward < 0.0) -centeredReward else 0.0
        nervousSystem.observe(
            consequence,
            duration = REWARD_STEPS,
            plasticityWrite = true,
            updateAssemblies = exposeAssemblies,
            useEligibility = useEligibility,
            useGeneratedOperator = useGeneratedOperator,
            exposeAssemblies = exposeAssemblies
        )

        if (persist) save()
    }

    override fun train(
        iterations: Int,
        random: Random,
        onProgress: (Int) -> Unit
    ) {
        if (!currentMode.trainable) return
        val boundedIterations = iterations.coerceIn(1, MAX_BATCH)
        repeat(boundedIterations) { index ->
            val playerCard = randomCard("training-player", random)
            val aiHand = List(RuleBook.HAND_SIZE) {
                randomCard("training-tatarus", random)
            }
            val weather = Weather.entries.random(random)
            val playerTokens = random.nextInt(1, 11)
            val aiTokens = random.nextInt(1, 11)
            val state = GameState(
                playerHand = listOf(playerCard),
                aiHand = aiHand,
                playerTokens = playerTokens,
                aiTokens = aiTokens,
                playerHero = Hero.entries.random(random),
                aiHero = Hero.entries.random(random)
            )
            val decision = choose(playerCard, aiHand, state, weather, random)
            val remainingAiHand =
                aiHand.filterNot { it.id in decision.consumedCardIds }
            val playerScore = RuleBook.combatScore(
                playerCard,
                decision.card,
                state.playerHero,
                playerTokens,
                aiTokens,
                weather,
                emptyList(),
                emptyList(),
                true
            )
            val aiScore = RuleBook.combatScore(
                decision.card,
                playerCard,
                state.aiHero,
                aiTokens,
                playerTokens,
                weather,
                remainingAiHand,
                emptyList(),
                false
            )
            val winner = RuleBook.winner(playerScore, aiScore)
            val reward = when (winner) {
                Winner.AI -> 1.0
                Winner.DRAW -> 0.5
                Winner.PLAYER -> 0.0
            }
            learn(
                decision = decision,
                reward = reward,
                persist = false,
                origin = LearningOrigin.SELF_TRAINING,
                outcome = winner
            )
            if ((index + 1) % 25 == 0 || index == boundedIterations - 1) {
                onProgress(index + 1)
            }
        }
        synchronized(this) {
            trainingRuns += 1
            save()
        }
    }

    @Synchronized
    override fun mode(): OpponentMode = currentMode

    @Synchronized
    override fun setMode(mode: OpponentMode) {
        currentMode = mode
        pendingFeatures.clear()
        save()
    }

    @Synchronized
    override fun evaluate(
        gamesPerMode: Int,
        modes: List<OpponentMode>,
        baseSeed: Int,
        onProgress: (Int, Int) -> Unit
    ): EvaluationSummary {
        val boundedGames = gamesPerMode.coerceIn(1, MAX_EVALUATION_GAMES)
        val selectedModes = modes.distinct()
        if (selectedModes.isEmpty()) return EvaluationSummary()
        val baseline = modelCheckpoint()
        val total = selectedModes.size * boundedGames
        var completed = 0
        val results = mutableListOf<EvaluationModeResult>()

        try {
            evaluationActive = true
            selectedModes.forEach { mode ->
                restoreModel(baseline)
                currentMode = mode
                evaluationActive = true
                val before = nervousSystem.metrics()
                var wins = 0
                var draws = 0
                var losses = 0
                var tokenSwing = 0.0
                var rounds = 0.0

                repeat(boundedGames) { gameIndex ->
                    val random = Random(baseSeed + gameIndex * EVALUATION_SEED_STRIDE)
                    val finalState = runEvaluationGame(random)
                    when {
                        finalState.aiTokens > finalState.playerTokens -> wins += 1
                        finalState.aiTokens == finalState.playerTokens -> draws += 1
                        else -> losses += 1
                    }
                    tokenSwing += finalState.aiTokens - finalState.playerTokens
                    rounds += finalState.history.size
                    completed += 1
                    onProgress(completed, total)
                }

                val after = nervousSystem.metrics()
                val spikeDelta = (after.spikes - before.spikes).coerceAtLeast(0)
                val transmissionDelta =
                    (after.transmissions - before.transmissions).coerceAtLeast(0)
                results += EvaluationModeResult(
                    mode = mode,
                    games = boundedGames,
                    wins = wins,
                    draws = draws,
                    losses = losses,
                    averageTokenSwing = tokenSwing / boundedGames,
                    averageRounds = rounds / boundedGames,
                    spikesPerGame = spikeDelta.toDouble() / boundedGames,
                    transmissionsPerGame = transmissionDelta.toDouble() / boundedGames,
                    energyCostPerGame =
                        (spikeDelta * SPIKE_COST +
                            transmissionDelta * TRANSMISSION_COST) / boundedGames
                )
            }
        } finally {
            restoreModel(baseline)
            evaluationActive = false
        }
        return EvaluationSummary(
            seeds = boundedGames,
            gamesPerMode = boundedGames,
            results = results
        )
    }

    private fun runEvaluationGame(random: Random): GameState {
        val engine = GameEngine(
            ai = this,
            random = random,
            learningOrigin = LearningOrigin.EVALUATION
        )
        var state = engine.newGame()
        var guard = 0
        while (state.phase != GamePhase.GAME_OVER && guard < EVALUATION_TURN_GUARD) {
            state = when (state.phase) {
                GamePhase.PLAYER_TURN -> {
                    val selectedId = evaluationPlayerCard(state, random)
                    engine.playCard(state, selectedId)
                }
                GamePhase.REVEAL -> engine.continueAfterReveal(state)
                GamePhase.GAME_OVER -> state
            }
            guard += 1
        }
        return state
    }

    private fun evaluationPlayerCard(state: GameState, random: Random): String {
        val fusionId = state.fusionSelectionId
        if (fusionId != null) {
            return state.playerHand.firstOrNull {
                it.id != fusionId && Mechanic.FUSION in it.mechanics
            }?.id ?: fusionId
        }
        return state.playerHand.random(random).id
    }

    private fun modelCheckpoint(): ModelSnapshot = ModelSnapshot(
        nervousSystem = nervousSystem.checkpoint(),
        readouts = readouts.mapValues { it.value.copyOf() },
        actionStats = actionStats.mapValues {
            ActionStats(it.value.visits, it.value.reward)
        },
        contextHashes = seenContextHashes.toSet(),
        pendingFeatures = pendingFeatures.mapValues { it.value.copyOf() },
        observations = observations,
        totalReward = totalReward,
        realObservations = realObservations,
        realReward = realReward,
        realWins = realWins,
        realDraws = realDraws,
        realLosses = realLosses,
        trainingObservations = trainingObservations,
        trainingReward = trainingReward,
        trainingRuns = trainingRuns,
        mode = currentMode
    )

    private fun restoreModel(snapshot: ModelSnapshot) {
        nervousSystem.restore(snapshot.nervousSystem)
        readouts.clear()
        readouts.putAll(snapshot.readouts.mapValues { it.value.copyOf() })
        actionStats.clear()
        actionStats.putAll(snapshot.actionStats.mapValues {
            ActionStats(it.value.visits, it.value.reward)
        })
        seenContextHashes.clear()
        seenContextHashes += snapshot.contextHashes
        pendingFeatures.clear()
        pendingFeatures.putAll(snapshot.pendingFeatures.mapValues { it.value.copyOf() })
        observations = snapshot.observations
        totalReward = snapshot.totalReward
        realObservations = snapshot.realObservations
        realReward = snapshot.realReward
        realWins = snapshot.realWins
        realDraws = snapshot.realDraws
        realLosses = snapshot.realLosses
        trainingObservations = snapshot.trainingObservations
        trainingReward = snapshot.trainingReward
        trainingRuns = snapshot.trainingRuns
        currentMode = snapshot.mode
    }

    @Synchronized
    override fun reset() {
        nervousSystem = TatarusNervousSystem()
        readouts.clear()
        actionStats.clear()
        seenContextHashes.clear()
        pendingFeatures.clear()
        observations = 0
        totalReward = 0.0
        realObservations = 0
        realReward = 0.0
        realWins = 0
        realDraws = 0
        realLosses = 0
        trainingObservations = 0
        trainingReward = 0.0
        trainingRuns = 0
        currentMode = OpponentMode.PURE_TATARUS
        preferences.edit()
            .remove(MODEL_KEY)
            .remove(LEGACY_MODEL_KEY_V2)
            .remove(LEGACY_MODEL_KEY_V1)
            .apply()
    }

    private fun createOptions(
        playerCard: RuneCard,
        hand: List<RuneCard>,
        state: GameState,
        weather: Weather
    ): List<Option> {
        val singles = hand.map { card ->
            Option(
                card = card,
                consumedIds = setOf(card.id),
                ruleScore = ruleScore(playerCard, card, hand, state, weather)
            )
        }
        val fusionCards = hand.filter { Mechanic.FUSION in it.mechanics }
        val fusions = buildList {
            for (firstIndex in 0 until fusionCards.lastIndex) {
                for (secondIndex in firstIndex + 1 until fusionCards.size) {
                    val first = fusionCards[firstIndex]
                    val second = fusionCards[secondIndex]
                    val fused = RuleBook.fuse(first, second)
                    val preview =
                        hand.filterNot { it.id == first.id || it.id == second.id }
                    add(
                        Option(
                            card = fused,
                            consumedIds = setOf(first.id, second.id),
                            ruleScore =
                                ruleScore(playerCard, fused, preview, state, weather) +
                                    FUSION_PRIOR
                        )
                    )
                }
            }
        }
        return singles + fusions
    }

    private fun ruleScore(
        playerCard: RuneCard,
        candidate: RuneCard,
        hand: List<RuneCard>,
        state: GameState,
        weather: Weather
    ): Double {
        val combat = RuleBook.combatScore(
            candidate,
            playerCard,
            state.aiHero,
            state.aiTokens,
            state.playerTokens,
            weather,
            hand.filterNot { it.id == candidate.id },
            state.history,
            false
        )
        var prospective = combat
        if (Mechanic.OVERLOAD in candidate.mechanics && state.aiTokens <= 2) {
            prospective -= 3.0
        }
        if (Mechanic.CHAIN in candidate.mechanics &&
            state.history.lastOrNull()?.aiCard?.mechanics?.contains(Mechanic.CHAIN) == true
        ) {
            prospective += 1.0
        }
        if (Mechanic.RESONANCE in candidate.mechanics) {
            prospective += hand.count { it.element == candidate.element } * 0.35
        }
        if (candidate.type == CardType.BLESSING_CURSE &&
            state.aiTokens < state.playerTokens
        ) {
            prospective += 1.0
        }
        return prospective
    }

    private fun encodeContext(
        playerCard: RuneCard,
        weather: Weather,
        state: GameState
    ): DoubleArray {
        val input = DoubleArray(TatarusNervousSystem.INPUT_SIZE)
        input[playerCard.element.ordinal] = 1.0
        input[10] = playerCard.ability.power / Ability.entries.lastIndex.toDouble()
        input[11] = playerCard.type.ordinal / CardType.entries.lastIndex.toDouble()
        input[12 + weather.ordinal] = 1.0
        input[15] = (state.playerTokens / 12.0).coerceIn(0.0, 1.0)
        input[16] = (state.aiTokens / 12.0).coerceIn(0.0, 1.0)
        input[17] =
            ((state.aiTokens - state.playerTokens + 10.0) / 20.0).coerceIn(0.0, 1.0)
        input[18] = state.playerHero.ordinal / max(1, Hero.entries.lastIndex).toDouble()
        input[19] = state.aiHero.ordinal / max(1, Hero.entries.lastIndex).toDouble()
        input[20] = (state.round / RuleBook.MAX_ROUNDS.toDouble()).coerceIn(0.0, 1.0)
        input[21] = if (state.history.lastOrNull()?.winner == Winner.AI) 1.0 else 0.0
        input[22] = if (state.history.lastOrNull()?.winner == Winner.PLAYER) 1.0 else 0.0
        input[23] = if (state.history.lastOrNull()?.winner == Winner.DRAW) 1.0 else 0.0
        Element.entries.take(8).forEachIndexed { index, element ->
            input[24 + index] =
                (state.history.count { it.playerCard.element == element } / 10.0)
                    .coerceIn(0.0, 1.0)
        }
        return input
    }

    private fun encodeCandidate(
        option: Option,
        playerCard: RuneCard,
        state: GameState,
        weather: Weather,
        includeDerivedRules: Boolean
    ): DoubleArray {
        val card = option.card
        val input = DoubleArray(TatarusNervousSystem.INPUT_SIZE)
        input[card.element.ordinal] = 1.0
        input[10] = card.ability.power / Ability.entries.lastIndex.toDouble()
        input[11] = card.type.ordinal / CardType.entries.lastIndex.toDouble()
        input[12] = if (card.fused) 1.0 else 0.0
        input[13] = card.mechanics.size / Mechanic.entries.size.toDouble()
        input[14] = if (includeDerivedRules) {
            ((RuleBook.elementAdvantage(card.element, playerCard.element) + 3.0) / 6.0)
                .coerceIn(0.0, 1.0)
        } else {
            0.0
        }
        input[15] = if (includeDerivedRules) {
            ((RuleBook.weatherModifier(weather, card.element) + 2.0) / 4.0)
                .coerceIn(0.0, 1.0)
        } else {
            0.0
        }
        input[16] = (state.aiTokens / 12.0).coerceIn(0.0, 1.0)
        input[17] = (state.playerTokens / 12.0).coerceIn(0.0, 1.0)
        input[18] = if (state.aiHero.element == card.element) 1.0 else 0.0
        input[19] = if (Mechanic.CHAIN in card.mechanics) 1.0 else 0.0
        input[20] = if (Mechanic.RESONANCE in card.mechanics) 1.0 else 0.0
        input[21] = if (Mechanic.OVERLOAD in card.mechanics) 1.0 else 0.0
        input[22] = if (Mechanic.FUSION in card.mechanics) 1.0 else 0.0
        input[23] = if (Mechanic.WEATHER_BOND in card.mechanics) 1.0 else 0.0
        input[24] = if (Mechanic.ALLY in card.mechanics) 1.0 else 0.0
        input[25] = if (Mechanic.BLESSING_CURSE in card.mechanics) 1.0 else 0.0
        input[26] = if (card.type == CardType.ARTIFACT) 1.0 else 0.0
        input[27] = if (card.type == CardType.SUMMON) 1.0 else 0.0
        input[28] = if (includeDerivedRules) {
            ((option.ruleScore + 10.0) / 35.0).coerceIn(0.0, 1.0)
        } else {
            0.0
        }
        input[29] = option.consumedIds.size / 2.0
        input[30] = state.history.takeLast(4).count {
            it.aiCard.element == card.element
        } / 4.0
        input[31] = 1.0
        return input
    }

    private fun policyFeatures(
        bridge: DoubleArray,
        candidate: DoubleArray
    ): DoubleArray {
        val result = DoubleArray(POLICY_FEATURES)
        bridge.copyInto(result, endIndex = TatarusNervousSystem.BRIDGE_SIZE)
        repeat(CANDIDATE_POLICY_FEATURES) { index ->
            result[TatarusNervousSystem.BRIDGE_SIZE + index] =
                candidate[CANDIDATE_POLICY_INDICES[index]]
        }
        return result
    }

    private fun createContextKey(
        playerCard: RuneCard,
        weather: Weather,
        state: GameState
    ): String {
        val deltaBucket = (state.playerTokens - state.aiTokens).coerceIn(-4, 4)
        val recent = state.history.takeLast(3)
            .joinToString(",") { "${it.playerCard.element.ordinal}:${it.winner.ordinal}" }
        return listOf(
            playerCard.element.name,
            playerCard.ability.name,
            weather.name,
            state.playerHero.name,
            state.aiHero.name,
            deltaBucket,
            recent
        ).joinToString("|")
    }

    private fun initialReadout(actionKey: String): DoubleArray {
        val random = Random(actionKey.hashCode() xor READOUT_SEED)
        return DoubleArray(POLICY_FEATURES) {
            random.nextDouble(-INITIAL_READOUT_SCALE, INITIAL_READOUT_SCALE)
        }
    }

    private fun dot(first: DoubleArray, second: DoubleArray): Double {
        var result = 0.0
        first.indices.forEach { result += first[it] * second[it] }
        return result
    }

    private fun explorationRate(): Double =
        (0.16 / sqrt(1.0 + observations / 2_000.0)).coerceAtLeast(0.035)

    private fun randomCard(owner: String, random: Random): RuneCard {
        val element = Element.entries.random(random)
        val ability = Ability.entries.random(random)
        val type =
            CardType.entries[(element.ordinal + ability.ordinal) % CardType.entries.size]
        return RuneCard(
            id = "$owner-${idCounter.incrementAndGet()}",
            element = element,
            ability = ability,
            type = type
        )
    }

    private fun pendingKey(contextKey: String, actionKey: String): String =
        "$contextKey::$actionKey"

    private fun stableHash64(value: String): Long {
        var hash = -3750763034362895579L
        value.forEach { character ->
            hash = (hash xor character.code.toLong()) * 1099511628211L
        }
        return hash
    }

    @Synchronized
    private fun save() {
        val readoutJson = JSONArray()
        readouts.forEach { (key, weights) ->
            readoutJson.put(
                JSONObject()
                    .put("key", key)
                    .put(
                        "weights",
                        JSONArray().also { array ->
                            weights.forEach { value -> array.put(value) }
                        }
                    )
            )
        }
        val statsJson = JSONArray()
        actionStats.forEach { (key, stats) ->
            statsJson.put(
                JSONObject()
                    .put("key", key)
                    .put("visits", stats.visits)
                    .put("reward", stats.reward)
            )
        }
        val contextsJson = JSONArray()
        seenContextHashes.take(MAX_PERSISTED_CONTEXTS)
            .forEach { contextHash -> contextsJson.put(contextHash) }

        val root = JSONObject()
            .put("version", MODEL_VERSION)
            .put("observations", observations)
            .put("totalReward", totalReward)
            .put("realObservations", realObservations)
            .put("realReward", realReward)
            .put("realWins", realWins)
            .put("realDraws", realDraws)
            .put("realLosses", realLosses)
            .put("trainingObservations", trainingObservations)
            .put("trainingReward", trainingReward)
            .put("trainingRuns", trainingRuns)
            .put("mode", currentMode.name)
            .put("contexts", contextsJson)
            .put("readouts", readoutJson)
            .put("actionStats", statsJson)
            .put("nervousSystem", nervousSystem.toJson())
        preferences.edit().putString(MODEL_KEY, root.toString()).apply()
    }

    @Synchronized
    private fun load() {
        val raw = preferences.getString(MODEL_KEY, null)
        if (raw == null) {
            preferences.edit()
                .remove(LEGACY_MODEL_KEY_V2)
                .remove(LEGACY_MODEL_KEY_V1)
                .apply()
            return
        }
        runCatching {
            val root = JSONObject(raw)
            require(root.optInt("version") == MODEL_VERSION)
            observations = root.optLong("observations")
            totalReward = root.optDouble("totalReward")
            realObservations = root.optLong("realObservations")
            realReward = root.optDouble("realReward")
            realWins = root.optLong("realWins")
            realDraws = root.optLong("realDraws")
            realLosses = root.optLong("realLosses")
            trainingObservations = root.optLong("trainingObservations")
            trainingReward = root.optDouble("trainingReward")
            trainingRuns = root.optInt("trainingRuns")
            currentMode = OpponentMode.valueOf(
                root.optString("mode", OpponentMode.PURE_TATARUS.name)
            )

            val contexts = root.optJSONArray("contexts") ?: JSONArray()
            repeat(contexts.length()) { seenContextHashes += contexts.getLong(it) }

            val readoutJson = root.optJSONArray("readouts") ?: JSONArray()
            repeat(readoutJson.length()) { index ->
                val item = readoutJson.getJSONObject(index)
                val values = item.getJSONArray("weights")
                require(values.length() == POLICY_FEATURES)
                readouts[item.getString("key")] =
                    DoubleArray(POLICY_FEATURES) { values.getDouble(it) }
            }

            val statsJson = root.optJSONArray("actionStats") ?: JSONArray()
            repeat(statsJson.length()) { index ->
                val item = statsJson.getJSONObject(index)
                actionStats[item.getString("key")] = ActionStats(
                    visits = item.optLong("visits"),
                    reward = item.optDouble("reward")
                )
            }
            nervousSystem.loadJson(root.getJSONObject("nervousSystem"))
        }.onFailure {
            nervousSystem = TatarusNervousSystem()
            readouts.clear()
            actionStats.clear()
            seenContextHashes.clear()
            observations = 0
            totalReward = 0.0
            realObservations = 0
            realReward = 0.0
            realWins = 0
            realDraws = 0
            realLosses = 0
            trainingObservations = 0
            trainingReward = 0.0
            trainingRuns = 0
            currentMode = OpponentMode.PURE_TATARUS
            preferences.edit()
                .remove(MODEL_KEY)
                .remove(LEGACY_MODEL_KEY_V2)
                .remove(LEGACY_MODEL_KEY_V1)
                .apply()
        }
    }

    companion object {
        private const val PREFS = "runenkrieg_tatarus"
        private const val MODEL_KEY = "tatarus_opponent_v3"
        private const val LEGACY_MODEL_KEY_V2 = "tatarus_opponent_v2"
        private const val LEGACY_MODEL_KEY_V1 = "tatarus_opponent_v1"
        private const val MODEL_VERSION = 3
        private const val MAX_BATCH = 5_000
        private const val MAX_PERSISTED_CONTEXTS = 50_000
        private const val MAX_EVALUATION_GAMES = 100
        private const val EVALUATION_SEED_STRIDE = 7_919
        private const val EVALUATION_TURN_GUARD = 400
        private const val NEURON_COUNT_FOR_RATE = 72.0
        private const val SPIKE_COST = 0.025
        private const val TRANSMISSION_COST = 0.0004

        private const val CONTEXT_STEPS = 10
        private const val CANDIDATE_STEPS = 6
        private const val REWARD_STEPS = 4
        private const val POLICY_FEATURES = 24
        private const val CANDIDATE_POLICY_FEATURES = 8
        private val CANDIDATE_POLICY_INDICES =
            intArrayOf(10, 12, 13, 14, 15, 18, 21, 28)

        private const val NEURAL_WEIGHT = 0.55
        private const val RULE_PRIOR_WEIGHT = 0.35
        private const val EMPIRICAL_WEIGHT = 0.10
        private const val POLICY_LEARNING_RATE = 0.035
        private const val READOUT_BOUND = 3.0
        private const val INITIAL_READOUT_SCALE = 0.025
        private const val READOUT_SEED = 0x54415255
        private const val FUSION_PRIOR = 1.25
    }
}
