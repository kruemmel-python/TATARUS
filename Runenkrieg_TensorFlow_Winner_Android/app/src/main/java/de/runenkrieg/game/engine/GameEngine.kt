package de.runenkrieg.game.engine

import de.runenkrieg.game.ai.GameOpponent
import de.runenkrieg.game.model.AiDecision
import de.runenkrieg.game.model.Element
import de.runenkrieg.game.model.GamePhase
import de.runenkrieg.game.model.GameState
import de.runenkrieg.game.model.Hero
import de.runenkrieg.game.model.LearningOrigin
import de.runenkrieg.game.model.Mechanic
import de.runenkrieg.game.model.RoundRecord
import de.runenkrieg.game.model.RuleBook
import de.runenkrieg.game.model.RuneCard
import de.runenkrieg.game.model.Weather
import de.runenkrieg.game.model.Winner
import kotlin.math.abs
import kotlin.math.max
import kotlin.random.Random

class GameEngine(
    private val ai: GameOpponent,
    private val random: Random = Random.Default,
    private val learningOrigin: LearningOrigin = LearningOrigin.REAL_GAME,
    private val persistLearning: Boolean = true
) {
    private var pendingDecision: AiDecision? = null

    fun newGame(): GameState {
        pendingDecision = null
        val deck = RuleBook.buildDeck(random)
        return GameState(
            deck = deck.drop(RuleBook.HAND_SIZE * 2),
            playerHand = deck.take(RuleBook.HAND_SIZE),
            aiHand = deck.drop(RuleBook.HAND_SIZE).take(RuleBook.HAND_SIZE),
            playerTokens = RuleBook.START_TOKENS,
            aiTokens = RuleBook.START_TOKENS,
            playerHero = Hero.entries.random(random),
            aiHero = Hero.entries.random(random),
            phase = GamePhase.PLAYER_TURN,
            status = "Du bist am Zug. Wähle eine Karte."
        )
    }

    fun playCard(state: GameState, cardId: String): GameState {
        if (state.phase != GamePhase.PLAYER_TURN) return state
        val selected = state.playerHand.firstOrNull { it.id == cardId } ?: return state

        state.fusionSelectionId?.let { selectedFusionId ->
            val first = state.playerHand.firstOrNull { it.id == selectedFusionId }
            if (first == null) return state.copy(fusionSelectionId = null)
            if (selected.id != first.id && Mechanic.FUSION !in selected.mechanics) {
                return state.copy(status = "Die zweite Karte muss ebenfalls die Fusion-Mechanik besitzen.")
            }
            if (selected.id != first.id) {
                val fused = RuleBook.fuse(first, selected)
                val reducedHand = state.playerHand.filterNot { it.id == first.id || it.id == selected.id } + fused
                val refill = refill(reducedHand, state.deck, "spieler")
                return state.copy(
                    playerHand = refill.hand,
                    deck = refill.deck,
                    fusionSelectionId = null,
                    status = "Fusion gelungen: ${fused.ability.label} (${fused.element.label}) ist bereit."
                )
            }
            return resolveRound(
                state.copy(
                    fusionSelectionId = null,
                    status = "Fusion abgebrochen – die Karte wird ausgespielt."
                ),
                selected
            )
        }

        if (Mechanic.FUSION in selected.mechanics &&
            state.playerHand.any { it.id != selected.id && Mechanic.FUSION in it.mechanics }
        ) {
            return state.copy(
                fusionSelectionId = selected.id,
                status = "Fusion vorbereitet. Wähle eine zweite Fusionskarte oder tippe erneut."
            )
        }
        return resolveRound(state, selected)
    }

    fun continueAfterReveal(state: GameState): GameState {
        if (state.phase != GamePhase.REVEAL) return state
        val playerRefill = refill(state.playerHand, state.deck, "spieler")
        val aiRefill = refill(state.aiHand, playerRefill.deck, "tatarus")

        val gameOver = state.playerTokens <= 0 ||
            state.aiTokens <= 0 ||
            state.history.size >= RuleBook.MAX_ROUNDS

        if (gameOver) {
            val winnerText = when {
                state.playerTokens > state.aiTokens -> "Du hast den Runenkrieg gewonnen!"
                state.aiTokens > state.playerTokens -> "TF-GEWINNER gewinnt dieses Duell."
                else -> "Das Duell endet unentschieden."
            }
            return state.copy(
                deck = aiRefill.deck,
                playerHand = playerRefill.hand,
                aiHand = aiRefill.hand,
                phase = GamePhase.GAME_OVER,
                status = winnerText,
                fusionSelectionId = null
            )
        }

        return state.copy(
            deck = aiRefill.deck,
            playerHand = playerRefill.hand,
            aiHand = aiRefill.hand,
            playerCard = null,
            aiCard = null,
            weather = null,
            roundWinner = null,
            phase = GamePhase.PLAYER_TURN,
            status = "Runde ${state.history.size + 1}: Wähle eine Karte.",
            fusionSelectionId = null,
            mechanicMessages = emptyList()
        )
    }

    private fun resolveRound(state: GameState, playerCard: RuneCard): GameState {
        val weather = Weather.entries.random(random)
        val playerHand = state.playerHand.filterNot { it.id == playerCard.id }
        val decision = ai.choose(playerCard, state.aiHand, state, weather, random)
        pendingDecision = decision
        val aiHand = state.aiHand.filterNot { it.id in decision.consumedCardIds }

        val playerScore = RuleBook.combatScore(
            playerCard, decision.card, state.playerHero, state.playerTokens, state.aiTokens,
            weather, playerHand, state.history, true
        )
        val aiScore = RuleBook.combatScore(
            decision.card, playerCard, state.aiHero, state.aiTokens, state.playerTokens,
            weather, aiHand, state.history, false
        )
        val winner = RuleBook.winner(playerScore, aiScore)
        var tokens = applyElementEffect(
            winner = winner,
            winnerCard = when (winner) {
                Winner.PLAYER -> playerCard
                Winner.AI -> decision.card
                Winner.DRAW -> null
            },
            playerTokens = state.playerTokens,
            aiTokens = state.aiTokens,
            round = state.round
        )
        val mechanics = applyMechanics(
            winner = winner,
            playerCard = playerCard,
            aiCard = decision.card,
            weather = weather,
            playerHand = playerHand,
            aiHand = aiHand,
            playerTokens = tokens.first,
            aiTokens = tokens.second,
            history = state.history
        )
        tokens = mechanics.playerTokens to mechanics.aiTokens

        val record = RoundRecord(
            round = state.round,
            playerCard = playerCard,
            aiCard = decision.card,
            weather = weather,
            winner = winner,
            playerTokens = tokens.first,
            aiTokens = tokens.second
        )
        val baseReward = when (winner) {
            Winner.AI -> 0.9
            Winner.DRAW -> 0.5
            Winner.PLAYER -> 0.1
        }
        val aiSwing = (tokens.second - state.aiTokens) - (tokens.first - state.playerTokens)
        ai.learn(
            decision = decision,
            reward = baseReward + aiSwing.coerceIn(-2, 2) * 0.05,
            persist = persistLearning,
            origin = learningOrigin,
            outcome = winner
        )

        val scoreText = "${formatScore(playerScore)} : ${formatScore(aiScore)}"
        val winnerText = when (winner) {
            Winner.PLAYER -> "Du gewinnst Runde ${state.round}"
            Winner.AI -> "TF-GEWINNER gewinnt Runde ${state.round}"
            Winner.DRAW -> "Runde ${state.round} endet unentschieden"
        }
        val learningText = if (decision.wasExploration) {
            "TF-GEWINNER hat eine neue Antwort erkundet."
        } else {
            "TF-GEWINNER nutzte seine bisher beste Strategie."
        }
        return state.copy(
            playerHand = playerHand,
            aiHand = aiHand,
            playerTokens = tokens.first,
            aiTokens = tokens.second,
            playerCard = playerCard,
            aiCard = decision.card,
            weather = weather,
            roundWinner = winner,
            phase = GamePhase.REVEAL,
            status = "$winnerText · Stärke Du/TF-GEWINNER $scoreText. $learningText",
            history = state.history + record,
            fusionSelectionId = null,
            mechanicMessages = mechanics.messages
        )
    }

    private fun applyElementEffect(
        winner: Winner,
        winnerCard: RuneCard?,
        playerTokens: Int,
        aiTokens: Int,
        round: Int
    ): Pair<Int, Int> {
        if (winner == Winner.DRAW || winnerCard == null) return playerTokens to aiTokens
        var player = playerTokens
        var aiValue = aiTokens
        val playerWon = winner == Winner.PLAYER

        fun damageOpponent(amount: Int = 1) {
            if (playerWon) aiValue -= amount else player -= amount
        }
        fun healWinner(amount: Int = 1) {
            if (playerWon) player += amount else aiValue += amount
        }

        when (winnerCard.element) {
            Element.FIRE, Element.ICE -> damageOpponent()
            Element.WATER -> {
                healWinner()
                damageOpponent()
            }
            Element.EARTH, Element.LIGHTNING -> healWinner()
            Element.AIR, Element.LIGHT -> healWinner(2)
            Element.MAGIC -> Unit
            Element.SHADOW -> {
                if (playerWon && aiValue > 0) {
                    aiValue -= 1
                    player += 1
                } else if (!playerWon && player > 0) {
                    player -= 1
                    aiValue += 1
                }
            }
            Element.CHAOS -> {
                if (round % 2 == 0) {
                    healWinner()
                    damageOpponent()
                } else {
                    if (playerWon) {
                        player -= 1
                        aiValue += 1
                    } else {
                        aiValue -= 1
                        player += 1
                    }
                }
            }
        }
        return max(0, player) to max(0, aiValue)
    }

    private data class MechanicResult(
        val playerTokens: Int,
        val aiTokens: Int,
        val messages: List<String>
    )

    private fun applyMechanics(
        winner: Winner,
        playerCard: RuneCard,
        aiCard: RuneCard,
        weather: Weather,
        playerHand: List<RuneCard>,
        aiHand: List<RuneCard>,
        playerTokens: Int,
        aiTokens: Int,
        history: List<RoundRecord>
    ): MechanicResult {
        var player = playerTokens
        var aiValue = aiTokens
        val messages = mutableListOf<String>()

        fun apply(card: RuneCard, playerOwned: Boolean, won: Boolean, remainingHand: List<RuneCard>) {
            fun own() = if (playerOwned) player else aiValue
            fun opponent() = if (playerOwned) aiValue else player
            fun setOwn(value: Int) {
                if (playerOwned) player = value else aiValue = value
            }
            fun setOpponent(value: Int) {
                if (playerOwned) aiValue = value else player = value
            }
            val actor = if (playerOwned) "Deine Karte" else "TF-GEWINNER"

            if (Mechanic.CHAIN in card.mechanics && won) {
                val last = history.lastOrNull()
                val previous = last?.let { if (playerOwned) it.playerCard else it.aiCard }
                val previousWin = last?.winner == if (playerOwned) Winner.PLAYER else Winner.AI
                if (previous != null && Mechanic.CHAIN in previous.mechanics && previousWin) {
                    setOpponent(max(0, opponent() - 1))
                    messages += "$actor löst einen Ketteneffekt aus (-1 gegnerischer Token)."
                }
            }
            if (Mechanic.RESONANCE in card.mechanics && won) {
                val count = history.count {
                    (if (playerOwned) it.playerCard else it.aiCard).element == card.element
                } + 1
                if (count >= 3) {
                    setOwn(own() + 1)
                    messages += "$actor erzeugt Elementarresonanz (+1 Token)."
                }
            }
            if (Mechanic.OVERLOAD in card.mechanics) {
                setOwn(max(0, own() - 1))
                messages += "$actor erleidet Überladung (-1 Token)."
            }
            if (Mechanic.WEATHER_BOND in card.mechanics) {
                val modifier = RuleBook.weatherModifier(weather, card.element).toInt()
                if (modifier != 0) {
                    setOwn(max(0, own() + modifier))
                    messages += "$actor bindet das Wetter (${if (modifier > 0) "+" else ""}$modifier Token)."
                }
            }
            if (Mechanic.ALLY in card.mechanics && remainingHand.any { it.element == card.element }) {
                setOwn(own() + 1)
                messages += "$actor ruft einen Verbündeten (+1 Token)."
            }
            if (Mechanic.BLESSING_CURSE in card.mechanics) {
                if (own() < opponent()) {
                    setOwn(own() + 1)
                    messages += "$actor erhält einen Segen (+1 Token)."
                } else {
                    setOpponent(max(0, opponent() - 1))
                    messages += "$actor spricht einen Fluch (-1 gegnerischer Token)."
                }
            }
        }

        apply(playerCard, true, winner == Winner.PLAYER, playerHand)
        apply(aiCard, false, winner == Winner.AI, aiHand)
        return MechanicResult(max(0, player), max(0, aiValue), messages)
    }

    private data class RefillResult(val hand: List<RuneCard>, val deck: List<RuneCard>)

    private fun refill(hand: List<RuneCard>, sourceDeck: List<RuneCard>, owner: String): RefillResult {
        val result = hand.toMutableList()
        val deck = sourceDeck.toMutableList()
        while (result.size < RuleBook.HAND_SIZE) {
            result += if (deck.isNotEmpty()) deck.removeAt(deck.lastIndex)
            else RuleBook.replacementCard(owner, random)
        }
        return RefillResult(result, deck)
    }

    private fun formatScore(value: Double): String =
        if (abs(value - value.toInt()) < 0.01) value.toInt().toString()
        else "%.1f".format(value)
}
