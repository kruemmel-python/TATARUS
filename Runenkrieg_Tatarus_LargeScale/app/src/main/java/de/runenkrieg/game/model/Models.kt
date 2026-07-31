package de.runenkrieg.game.model

enum class Element(val label: String, val symbol: String) {
    FIRE("Feuer", "🔥"),
    WATER("Wasser", "💧"),
    EARTH("Erde", "🌱"),
    AIR("Luft", "🌬"),
    LIGHTNING("Blitz", "⚡"),
    ICE("Eis", "❄"),
    MAGIC("Magie", "✨"),
    SHADOW("Schatten", "◐"),
    LIGHT("Licht", "☀"),
    CHAOS("Chaos", "🌀")
}

enum class Mechanic(val label: String, val description: String, val weight: Double) {
    CHAIN("Ketteneffekte", "Siege in Folge entziehen einen zusätzlichen Token.", 1.5),
    RESONANCE("Elementarresonanz", "Wiederholte Elemente erzeugen einen Bonus.", 2.0),
    OVERLOAD("Überladung", "Hohe Kraft, aber der Einsatz kostet einen Token.", 2.5),
    FUSION("Fusion", "Zwei Fusionskarten können zu einer Karte verschmelzen.", 3.0),
    WEATHER_BOND("Wetterbindung", "Das aktuelle Wetter wirkt besonders stark.", 1.8),
    ALLY("Verbündeter", "Gleiche Elemente auf der Hand geben Unterstützung.", 1.3),
    BLESSING_CURSE("Segen/Fluch", "Heilt bei Rückstand, sonst wird der Gegner geschwächt.", 1.1)
}

enum class Ability(val label: String, val power: Int, val mechanics: Set<Mechanic>) {
    SPARK("Funke", 0, setOf(Mechanic.CHAIN)),
    RAY("Strahl", 1, setOf(Mechanic.WEATHER_BOND)),
    FLAME("Flamme", 2, setOf(Mechanic.RESONANCE)),
    EMBER("Glut", 3, setOf(Mechanic.CHAIN)),
    FIREBALL("Feuerball", 4, setOf(Mechanic.OVERLOAD)),
    INFERNO("Inferno", 5, setOf(Mechanic.RESONANCE, Mechanic.OVERLOAD)),
    NOVA("Nova", 6, setOf(Mechanic.FUSION)),
    SUPERNOVA("Supernova", 7, setOf(Mechanic.FUSION, Mechanic.OVERLOAD)),
    APOCALYPSE("Apokalypse", 8, setOf(Mechanic.FUSION, Mechanic.RESONANCE)),
    WORLD_FIRE("Weltenbrand", 9, setOf(Mechanic.FUSION, Mechanic.OVERLOAD, Mechanic.CHAIN)),
    ACOLYTE("Akolyth", 10, setOf(Mechanic.ALLY)),
    PRIESTESS("Priesterin", 11, setOf(Mechanic.BLESSING_CURSE)),
    ELEMENTAL("Elementar", 12, setOf(Mechanic.RESONANCE, Mechanic.WEATHER_BOND)),
    AVATAR("Avatar", 13, setOf(Mechanic.FUSION, Mechanic.RESONANCE, Mechanic.WEATHER_BOND));

    companion object {
        fun fromPower(power: Int): Ability = entries[power.coerceIn(0, entries.lastIndex)]
    }
}

enum class CardType(val label: String, val defaultLifespan: Int? = null, val defaultCharges: Int? = null) {
    ARTIFACT("Artefakt"),
    SUMMON("Beschwörung", defaultLifespan = 3),
    RUNESTONE("Runenstein", defaultCharges = 1),
    ALLY("Verbündeter"),
    BLESSING_CURSE("Segen/Fluch", defaultLifespan = 2)
}

data class RuneCard(
    val id: String,
    val element: Element,
    val ability: Ability,
    val type: CardType,
    val mechanics: Set<Mechanic> = ability.mechanics,
    val lifespan: Int? = type.defaultLifespan,
    val charges: Int? = type.defaultCharges,
    val fused: Boolean = false
) {
    val learningKey: String
        get() = "${element.name}|${ability.name}|${type.name}|${if (fused) 1 else 0}"
}

enum class Weather(val label: String, val symbol: String) {
    RAIN("Regen", "🌧"),
    STORM("Windsturm", "🌪"),
    EARTHQUAKE("Erdbeben", "〰")
}

enum class Hero(val label: String, val element: Element, val bonus: Int) {
    DRAGON("Drache", Element.FIRE, 2),
    WIZARD("Zauberer", Element.MAGIC, 3)
}

enum class Winner { PLAYER, AI, DRAW }
enum class GamePhase { PLAYER_TURN, REVEAL, GAME_OVER }

data class RoundRecord(
    val round: Int,
    val playerCard: RuneCard,
    val aiCard: RuneCard,
    val weather: Weather,
    val winner: Winner,
    val playerTokens: Int,
    val aiTokens: Int
)

data class GameState(
    val deck: List<RuneCard> = emptyList(),
    val playerHand: List<RuneCard> = emptyList(),
    val aiHand: List<RuneCard> = emptyList(),
    val playerTokens: Int = 5,
    val aiTokens: Int = 5,
    val playerHero: Hero = Hero.DRAGON,
    val aiHero: Hero = Hero.WIZARD,
    val playerCard: RuneCard? = null,
    val aiCard: RuneCard? = null,
    val weather: Weather? = null,
    val roundWinner: Winner? = null,
    val phase: GamePhase = GamePhase.PLAYER_TURN,
    val status: String = "Wähle eine Karte.",
    val history: List<RoundRecord> = emptyList(),
    val fusionSelectionId: String? = null,
    val mechanicMessages: List<String> = emptyList()
) {
    val round: Int get() = history.size + 1
}

data class AiContext(
    val key: String,
    val playerCard: RuneCard,
    val weather: Weather,
    val playerTokens: Int,
    val aiTokens: Int
)

data class AiDecision(
    val card: RuneCard,
    val consumedCardIds: Set<String>,
    val contextKey: String,
    val actionKey: String,
    val learnedScore: Double,
    val heuristicScore: Double,
    val wasExploration: Boolean
)

enum class OpponentMode(
    val label: String,
    val description: String,
    val trainable: Boolean
) {
    PURE_TATARUS(
        "Reines TATARUS",
        "Nur neuronaler Zustand und gelernter Readout; keine Regel- oder Erfahrungsbeimischung.",
        true
    ),
    HYBRID(
        "Hybrid 55/35/10",
        "Neuronaler Score plus Regelprior und mittlere Aktionserfahrung.",
        true
    ),
    RULE_ONLY(
        "Nur Regeln",
        "Deterministische Runenkrieg-Heuristik ohne neuronales Lernen.",
        false
    ),
    RANDOM(
        "Zufall",
        "Zufällige legale Einzelkarte oder Fusion.",
        false
    ),
    FROZEN_TATARUS(
        "TATARUS eingefroren",
        "Neuronale Entscheidung ohne Gewichts-, Eligibility- oder Assembly-Updates.",
        false
    ),
    NO_ELIGIBILITY(
        "Ohne Eligibility",
        "TATARUS ohne Eligibility-Erzeugung, Übertragungsmodulation und belohnte Synapsenänderung.",
        true
    ),
    NO_OPERATOR(
        "Ohne Generated Operator",
        "TATARUS mit konstantem Gate 0,5 statt Generated Operator.",
        true
    ),
    NO_ASSEMBLIES(
        "Ohne Assemblies",
        "TATARUS ohne Assembly-Update und ohne Assemblymerkmale im Readout.",
        true
    )
}

enum class LearningOrigin { REAL_GAME, SELF_TRAINING, EVALUATION }

data class EvaluationModeResult(
    val mode: OpponentMode,
    val games: Int,
    val wins: Int,
    val draws: Int,
    val losses: Int,
    val averageTokenSwing: Double,
    val averageRounds: Double,
    val spikesPerGame: Double,
    val transmissionsPerGame: Double,
    val energyCostPerGame: Double,
    val averageDecisionMillis: Double
) {
    val winRate: Double
        get() = if (games == 0) 0.0 else wins.toDouble() / games
}

data class EvaluationSummary(
    val seeds: Int = 0,
    val gamesPerMode: Int = 0,
    val results: List<EvaluationModeResult> = emptyList()
)

data class LearningSummary(
    val observations: Long = 0,
    val contexts: Long = 0,
    val actions: Int = 0,
    val averageReward: Double = 0.0,
    val explorationRate: Double = 0.18,
    val trainingRuns: Int = 0,
    val mode: OpponentMode = OpponentMode.PURE_TATARUS,
    val realObservations: Long = 0,
    val realAverageReward: Double = 0.0,
    val realWins: Long = 0,
    val realDraws: Long = 0,
    val realLosses: Long = 0,
    val trainingObservations: Long = 0,
    val trainingAverageReward: Double = 0.0,
    val neuralSteps: Long = 0,
    val neuralSpikes: Long = 0,
    val spikeRateHz: Double = 0.0,
    val transmissions: Long = 0,
    val totalNeurons: Int = 0,
    val inputChannels: Int = 0,
    val inputProjections: Int = 0,
    val totalSynapses: Int = 0,
    val stateBytesEstimate: Long = 0,
    val measuredDecisions: Long = 0,
    val averageDecisionMillis: Double = 0.0,
    val recentlyActiveSynapses: Int = 0,
    val saturatedWeightFraction: Double = 0.0,
    val activeAssemblies: Int = 0,
    val assemblyEntropy: Double = 0.0,
    val assemblySeparation: Double = 0.0,
    val assemblyReactivations: Long = 0,
    val meanEnergy: Double = 1.0,
    val minimumEnergy: Double = 1.0,
    val energyP10: Double = 1.0,
    val energyCostPerObservation: Double = 0.0,
    val meanEligibility: Double = 0.0,
    val meanAbsoluteEligibility: Double = 0.0,
    val eligibilityStdDev: Double = 0.0,
    val maximumAbsoluteEligibility: Double = 0.0,
    val activeEligibilityFraction: Double = 0.0,
    val saturatedEligibilityFraction: Double = 0.0,
    val positiveEligibilityFraction: Double = 0.0,
    val negativeEligibilityFraction: Double = 0.0
)
