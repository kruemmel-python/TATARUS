package de.runenkrieg.game.model

import java.util.concurrent.atomic.AtomicLong
import kotlin.math.floor
import kotlin.math.max
import kotlin.random.Random

object RuleBook {
    const val START_TOKENS = 5
    const val HAND_SIZE = 4
    const val MAX_ROUNDS = 100

    private val generatedCounter = AtomicLong()

    private val elementHierarchy: Map<Element, Map<Element, Double>> = mapOf(
        Element.WATER to mapOf(Element.FIRE to 3.0, Element.EARTH to 1.0, Element.AIR to -3.0, Element.LIGHTNING to -3.0, Element.ICE to 3.0, Element.CHAOS to -2.0),
        Element.FIRE to mapOf(Element.EARTH to 3.0, Element.AIR to 1.0, Element.WATER to -3.0, Element.ICE to 1.0, Element.LIGHTNING to 1.0, Element.SHADOW to 2.0),
        Element.EARTH to mapOf(Element.AIR to 3.0, Element.WATER to -1.0, Element.FIRE to -3.0, Element.LIGHTNING to 3.0, Element.ICE to 1.0, Element.CHAOS to 1.0),
        Element.AIR to mapOf(Element.WATER to 3.0, Element.EARTH to -1.0, Element.FIRE to -3.0, Element.ICE to 3.0, Element.LIGHTNING to -1.0, Element.SHADOW to -2.0),
        Element.LIGHTNING to mapOf(Element.WATER to 3.0, Element.EARTH to 1.0, Element.FIRE to 1.0, Element.AIR to -3.0, Element.ICE to -1.0, Element.SHADOW to 2.0, Element.CHAOS to -1.0),
        Element.ICE to mapOf(Element.FIRE to 3.0, Element.EARTH to 1.0, Element.WATER to -3.0, Element.AIR to 1.0, Element.LIGHTNING to 3.0, Element.CHAOS to -2.0),
        Element.MAGIC to mapOf(Element.FIRE to 1.0, Element.WATER to 1.0, Element.EARTH to 1.0, Element.AIR to 1.0, Element.LIGHTNING to 2.0, Element.ICE to 2.0, Element.SHADOW to 3.0, Element.LIGHT to -2.0),
        Element.SHADOW to mapOf(Element.LIGHT to 3.0, Element.MAGIC to -2.0, Element.CHAOS to 1.0),
        Element.LIGHT to mapOf(Element.SHADOW to 3.0, Element.MAGIC to 2.0, Element.CHAOS to -1.0),
        Element.CHAOS to mapOf(Element.MAGIC to 1.0, Element.LIGHT to 2.0, Element.SHADOW to -2.0, Element.FIRE to -1.0, Element.LIGHTNING to 2.0)
    )

    data class ElementSynergy(
        val first: Element,
        val second: Element,
        val modifier: Double
    ) {
        fun contains(element: Element) = element == first || element == second
        fun partnerOf(element: Element) = if (element == first) second else first
    }

    val synergies = listOf(
        ElementSynergy(Element.WATER, Element.LIGHTNING, 2.0),
        ElementSynergy(Element.FIRE, Element.EARTH, 1.5),
        ElementSynergy(Element.LIGHT, Element.SHADOW, 2.5),
        ElementSynergy(Element.ICE, Element.AIR, 1.2),
        ElementSynergy(Element.EARTH, Element.LIGHT, 1.8)
    )

    fun elementAdvantage(attacker: Element, defender: Element): Double =
        elementHierarchy[attacker]?.get(defender) ?: 0.0

    fun weatherModifier(weather: Weather, element: Element): Double = when (weather) {
        Weather.RAIN -> when (element) {
            Element.WATER -> 1.0
            Element.FIRE -> -1.0
            else -> 0.0
        }
        Weather.STORM -> when (element) {
            Element.AIR -> 2.0
            Element.EARTH -> -1.0
            else -> 0.0
        }
        Weather.EARTHQUAKE -> 0.0
    }

    fun buildDeck(random: Random = Random.Default): List<RuneCard> =
        Element.entries.flatMapIndexed { elementIndex, element ->
            Ability.entries.mapIndexed { abilityIndex, ability ->
                val type = CardType.entries[(elementIndex + abilityIndex) % CardType.entries.size]
                RuneCard(
                    id = "${element.name}-${ability.name}-$elementIndex-$abilityIndex",
                    element = element,
                    ability = ability,
                    type = type
                )
            }
        }.shuffled(random)

    fun replacementCard(owner: String, random: Random = Random.Default): RuneCard {
        val element = Element.entries.random(random)
        val ability = Ability.entries.random(random)
        val type = CardType.entries[(element.ordinal + ability.ordinal) % CardType.entries.size]
        return RuneCard(
            id = "${element.name}-${ability.name}-$owner-${generatedCounter.incrementAndGet()}",
            element = element,
            ability = ability,
            type = type
        )
    }

    fun fuse(first: RuneCard, second: RuneCard): RuneCard {
        val ability = Ability.fromPower(first.ability.power + second.ability.power)
        val synergy = synergies.firstOrNull { it.contains(first.element) && it.contains(second.element) }
        val element = when {
            synergy != null -> first.element
            first.ability.power >= second.ability.power -> first.element
            else -> second.element
        }
        val lifespan = max(first.lifespan ?: 0, second.lifespan ?: 0)
            .takeIf { it > 0 }?.plus(1)
        val charges = ((first.charges ?: 0) + (second.charges ?: 0)).takeIf { it > 0 }
        return RuneCard(
            id = "fusion-${first.id}-${second.id}-${generatedCounter.incrementAndGet()}",
            element = element,
            ability = ability,
            type = if (first.type == second.type) first.type else CardType.SUMMON,
            mechanics = first.mechanics + second.mechanics + Mechanic.FUSION,
            lifespan = lifespan,
            charges = charges,
            fused = true
        )
    }

    fun riskAndWeather(
        card: RuneCard,
        ownTokens: Int,
        opponentTokens: Int,
        weather: Weather
    ): Double {
        val weatherBonus = weatherModifier(weather, card.element)
        var result = weatherBonus
        if (Mechanic.OVERLOAD in card.mechanics) {
            result += if (opponentTokens - ownTokens >= 2) 2.0 else -1.0
        }
        if (Mechanic.WEATHER_BOND in card.mechanics) {
            result += if (weatherBonus >= 0) weatherBonus + 1 else weatherBonus - 1
        }
        if (card.type == CardType.BLESSING_CURSE) {
            result += if (ownTokens < opponentTokens) 1.5 else -0.5
        }
        if (card.type == CardType.ARTIFACT) result += 0.5
        if (card.type == CardType.SUMMON && card.lifespan != null) {
            result += max(0, 4 - card.lifespan) * 0.25
        }
        return result
    }

    fun synergyBonus(
        card: RuneCard,
        hand: List<RuneCard>,
        history: List<RoundRecord>,
        ownerIsPlayer: Boolean
    ): Double {
        fun ownedCard(record: RoundRecord) = if (ownerIsPlayer) record.playerCard else record.aiCard
        var bonus = 0.0
        if (Mechanic.RESONANCE in card.mechanics) {
            val stacks = history.count { ownedCard(it).element == card.element } +
                hand.count { it.element == card.element }
            if (stacks >= 2) bonus += 2.0 + 0.5 * (stacks - 2)
        }
        synergies.filter { it.contains(card.element) }.forEach { synergy ->
            val partner = synergy.partnerOf(card.element)
            if (history.any { ownedCard(it).element == partner } || hand.any { it.element == partner }) {
                bonus += synergy.modifier
            }
        }
        if (Mechanic.FUSION in card.mechanics &&
            hand.any { it.element != card.element && Mechanic.FUSION in it.mechanics }
        ) {
            bonus += 1.0 + hand.count {
                it.element != card.element && Mechanic.FUSION in it.mechanics
            } * 0.5
        }
        if (Mechanic.CHAIN in card.mechanics &&
            history.lastOrNull()?.let { Mechanic.CHAIN in ownedCard(it).mechanics } == true
        ) {
            bonus += 1.5
        }
        return bonus
    }

    fun combatScore(
        card: RuneCard,
        opponentCard: RuneCard,
        hero: Hero,
        ownTokens: Int,
        opponentTokens: Int,
        weather: Weather,
        hand: List<RuneCard>,
        history: List<RoundRecord>,
        ownerIsPlayer: Boolean
    ): Double = card.ability.power +
        riskAndWeather(card, ownTokens, opponentTokens, weather) +
        elementAdvantage(card.element, opponentCard.element) +
        (if (hero.element == card.element) hero.bonus else 0) +
        floor(max(0, ownTokens - opponentTokens) / 2.0).coerceAtMost(4.0) +
        synergyBonus(card, hand, history, ownerIsPlayer)

    fun winner(playerScore: Double, aiScore: Double): Winner = when {
        playerScore > aiScore -> Winner.PLAYER
        aiScore > playerScore -> Winner.AI
        else -> Winner.DRAW
    }
}
