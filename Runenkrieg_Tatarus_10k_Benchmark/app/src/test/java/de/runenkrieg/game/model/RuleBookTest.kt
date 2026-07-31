package de.runenkrieg.game.model

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.random.Random

class RuleBookTest {
    @Test
    fun deckContainsEveryElementAbilityCombinationExactlyOnce() {
        val deck = RuleBook.buildDeck(Random(42))

        assertEquals(Element.entries.size * Ability.entries.size, deck.size)
        assertEquals(deck.size, deck.map { it.element to it.ability }.toSet().size)
        assertEquals(deck.size, deck.map { it.id }.toSet().size)
    }

    @Test
    fun elementHierarchyKeepsOriginalWaterFireCounter() {
        assertEquals(3.0, RuleBook.elementAdvantage(Element.WATER, Element.FIRE), 0.0)
        assertEquals(-3.0, RuleBook.elementAdvantage(Element.FIRE, Element.WATER), 0.0)
    }

    @Test
    fun fusionCapsPowerAtAvatarAndMergesMechanics() {
        val first = RuneCard(
            id = "one",
            element = Element.FIRE,
            ability = Ability.WORLD_FIRE,
            type = CardType.ARTIFACT
        )
        val second = RuneCard(
            id = "two",
            element = Element.EARTH,
            ability = Ability.APOCALYPSE,
            type = CardType.RUNESTONE
        )

        val fused = RuleBook.fuse(first, second)

        assertEquals(Ability.AVATAR, fused.ability)
        assertEquals(Element.FIRE, fused.element)
        assertEquals(CardType.SUMMON, fused.type)
        assertTrue(fused.fused)
        assertTrue(Mechanic.FUSION in fused.mechanics)
        assertTrue(Mechanic.CHAIN in fused.mechanics)
        assertTrue(Mechanic.RESONANCE in fused.mechanics)
    }

    @Test
    fun rainStrengthensWaterAndWeakensFire() {
        assertEquals(1.0, RuleBook.weatherModifier(Weather.RAIN, Element.WATER), 0.0)
        assertEquals(-1.0, RuleBook.weatherModifier(Weather.RAIN, Element.FIRE), 0.0)
        assertEquals(0.0, RuleBook.weatherModifier(Weather.RAIN, Element.MAGIC), 0.0)
    }

    @Test
    fun strongerScoreWinsAndEqualScoreDraws() {
        assertEquals(Winner.PLAYER, RuleBook.winner(10.0, 4.0))
        assertEquals(Winner.AI, RuleBook.winner(2.0, 3.0))
        assertEquals(Winner.DRAW, RuleBook.winner(7.5, 7.5))
    }
}
