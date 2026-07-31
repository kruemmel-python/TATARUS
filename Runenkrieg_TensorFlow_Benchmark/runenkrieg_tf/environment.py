from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
import math
import random
from typing import Iterable, Sequence


class Element(IntEnum):
    FIRE = 0
    WATER = 1
    EARTH = 2
    AIR = 3
    LIGHTNING = 4
    ICE = 5
    MAGIC = 6
    SHADOW = 7
    LIGHT = 8
    CHAOS = 9


class Mechanic(IntEnum):
    CHAIN = 0
    RESONANCE = 1
    OVERLOAD = 2
    FUSION = 3
    WEATHER_BOND = 4
    ALLY = 5
    BLESSING_CURSE = 6


class CardType(IntEnum):
    ARTIFACT = 0
    SUMMON = 1
    RUNESTONE = 2
    ALLY = 3
    BLESSING_CURSE = 4


class Weather(IntEnum):
    RAIN = 0
    STORM = 1
    EARTHQUAKE = 2


class Winner(IntEnum):
    PLAYER = 0
    AI = 1
    DRAW = 2


ABILITY_MECHANICS: tuple[frozenset[Mechanic], ...] = (
    frozenset((Mechanic.CHAIN,)),
    frozenset((Mechanic.WEATHER_BOND,)),
    frozenset((Mechanic.RESONANCE,)),
    frozenset((Mechanic.CHAIN,)),
    frozenset((Mechanic.OVERLOAD,)),
    frozenset((Mechanic.RESONANCE, Mechanic.OVERLOAD)),
    frozenset((Mechanic.FUSION,)),
    frozenset((Mechanic.FUSION, Mechanic.OVERLOAD)),
    frozenset((Mechanic.FUSION, Mechanic.RESONANCE)),
    frozenset((Mechanic.FUSION, Mechanic.OVERLOAD, Mechanic.CHAIN)),
    frozenset((Mechanic.ALLY,)),
    frozenset((Mechanic.BLESSING_CURSE,)),
    frozenset((Mechanic.RESONANCE, Mechanic.WEATHER_BOND)),
    frozenset((Mechanic.FUSION, Mechanic.RESONANCE, Mechanic.WEATHER_BOND)),
)


@dataclass(frozen=True)
class Card:
    uid: str
    element: Element
    power: int
    card_type: CardType
    mechanics: frozenset[Mechanic]
    fused: bool = False
    lifespan: int | None = None
    charges: int | None = None


@dataclass(frozen=True)
class Action:
    card: Card
    consumed: tuple[str, ...]


@dataclass(frozen=True)
class RoundRecord:
    player_card: Card
    ai_card: Card
    weather: Weather
    winner: Winner
    player_tokens: int
    ai_tokens: int


@dataclass
class GameState:
    deck: list[Card]
    player_hand: list[Card]
    ai_hand: list[Card]
    player_tokens: int = 5
    ai_tokens: int = 5
    player_hero: Element = Element.FIRE
    ai_hero: Element = Element.MAGIC
    history: list[RoundRecord] = field(default_factory=list)

    @property
    def done(self) -> bool:
        return (
            self.player_tokens <= 0
            or self.ai_tokens <= 0
            or len(self.history) >= 100
        )


ELEMENT_ADVANTAGE: dict[Element, dict[Element, float]] = {
    Element.WATER: {Element.FIRE: 3, Element.EARTH: 1, Element.AIR: -3, Element.LIGHTNING: -3, Element.ICE: 3, Element.CHAOS: -2},
    Element.FIRE: {Element.EARTH: 3, Element.AIR: 1, Element.WATER: -3, Element.ICE: 1, Element.LIGHTNING: 1, Element.SHADOW: 2},
    Element.EARTH: {Element.AIR: 3, Element.WATER: -1, Element.FIRE: -3, Element.LIGHTNING: 3, Element.ICE: 1, Element.CHAOS: 1},
    Element.AIR: {Element.WATER: 3, Element.EARTH: -1, Element.FIRE: -3, Element.ICE: 3, Element.LIGHTNING: -1, Element.SHADOW: -2},
    Element.LIGHTNING: {Element.WATER: 3, Element.EARTH: 1, Element.FIRE: 1, Element.AIR: -3, Element.ICE: -1, Element.SHADOW: 2, Element.CHAOS: -1},
    Element.ICE: {Element.FIRE: 3, Element.EARTH: 1, Element.WATER: -3, Element.AIR: 1, Element.LIGHTNING: 3, Element.CHAOS: -2},
    Element.MAGIC: {Element.FIRE: 1, Element.WATER: 1, Element.EARTH: 1, Element.AIR: 1, Element.LIGHTNING: 2, Element.ICE: 2, Element.SHADOW: 3, Element.LIGHT: -2},
    Element.SHADOW: {Element.LIGHT: 3, Element.MAGIC: -2, Element.CHAOS: 1},
    Element.LIGHT: {Element.SHADOW: 3, Element.MAGIC: 2, Element.CHAOS: -1},
    Element.CHAOS: {Element.MAGIC: 1, Element.LIGHT: 2, Element.SHADOW: -2, Element.FIRE: -1, Element.LIGHTNING: 2},
}

SYNERGIES = (
    (Element.WATER, Element.LIGHTNING, 2.0),
    (Element.FIRE, Element.EARTH, 1.5),
    (Element.LIGHT, Element.SHADOW, 2.5),
    (Element.ICE, Element.AIR, 1.2),
    (Element.EARTH, Element.LIGHT, 1.8),
)


def make_card(uid: str, element: Element, power: int) -> Card:
    card_type = CardType((element.value + power) % len(CardType))
    return Card(
        uid,
        element,
        power,
        card_type,
        ABILITY_MECHANICS[power],
        lifespan=3 if card_type == CardType.SUMMON else 2 if card_type == CardType.BLESSING_CURSE else None,
        charges=1 if card_type == CardType.RUNESTONE else None,
    )


def build_deck(rng: random.Random) -> list[Card]:
    deck = [
        make_card(f"{element.name}-{power}", element, power)
        for element in Element
        for power in range(14)
    ]
    rng.shuffle(deck)
    return deck


def fuse(first: Card, second: Card) -> Card:
    power = min(13, first.power + second.power)
    synergy = any(
        first.element in pair[:2] and second.element in pair[:2]
        for pair in SYNERGIES
    )
    element = first.element if synergy or first.power >= second.power else second.element
    card_type = first.card_type if first.card_type == second.card_type else CardType.SUMMON
    return Card(
        f"fusion-{first.uid}-{second.uid}",
        element,
        power,
        card_type,
        first.mechanics | second.mechanics | frozenset((Mechanic.FUSION,)),
        True,
        max(first.lifespan or 0, second.lifespan or 0) + 1
        if max(first.lifespan or 0, second.lifespan or 0) > 0
        else None,
        (first.charges or 0) + (second.charges or 0)
        if (first.charges or 0) + (second.charges or 0) > 0
        else None,
    )


def legal_actions(hand: Sequence[Card]) -> list[Action]:
    actions = [Action(card, (card.uid,)) for card in hand]
    fusion_cards = [card for card in hand if Mechanic.FUSION in card.mechanics]
    for first_index, first in enumerate(fusion_cards[:-1]):
        for second in fusion_cards[first_index + 1 :]:
            actions.append(Action(fuse(first, second), (first.uid, second.uid)))
    return actions


def weather_modifier(weather: Weather, element: Element) -> float:
    if weather == Weather.RAIN:
        return 1.0 if element == Element.WATER else -1.0 if element == Element.FIRE else 0.0
    if weather == Weather.STORM:
        return 2.0 if element == Element.AIR else -1.0 if element == Element.EARTH else 0.0
    return 0.0


def element_advantage(attacker: Element, defender: Element, rule_sign: float = 1.0) -> float:
    return rule_sign * ELEMENT_ADVANTAGE.get(attacker, {}).get(defender, 0.0)


def synergy_bonus(card: Card, hand: Sequence[Card], history: Sequence[RoundRecord], player: bool) -> float:
    owned = lambda record: record.player_card if player else record.ai_card
    bonus = 0.0
    if Mechanic.RESONANCE in card.mechanics:
        stacks = sum(owned(record).element == card.element for record in history)
        stacks += sum(candidate.element == card.element for candidate in hand)
        if stacks >= 2:
            bonus += 2.0 + 0.5 * (stacks - 2)
    for first, second, modifier in SYNERGIES:
        if card.element not in (first, second):
            continue
        partner = second if card.element == first else first
        if any(owned(record).element == partner for record in history) or any(
            candidate.element == partner for candidate in hand
        ):
            bonus += modifier
    if Mechanic.FUSION in card.mechanics:
        partners = sum(
            candidate.element != card.element and Mechanic.FUSION in candidate.mechanics
            for candidate in hand
        )
        if partners:
            bonus += 1.0 + 0.5 * partners
    if (
        Mechanic.CHAIN in card.mechanics
        and history
        and Mechanic.CHAIN in owned(history[-1]).mechanics
    ):
        bonus += 1.5
    return bonus


def combat_score(
    card: Card,
    opponent: Card,
    hero: Element,
    own_tokens: int,
    opponent_tokens: int,
    weather: Weather,
    hand: Sequence[Card],
    history: Sequence[RoundRecord],
    player: bool,
    rule_sign: float,
) -> float:
    weather_bonus = weather_modifier(weather, card.element)
    risk = weather_bonus
    if Mechanic.OVERLOAD in card.mechanics:
        risk += 2.0 if opponent_tokens - own_tokens >= 2 else -1.0
    if Mechanic.WEATHER_BOND in card.mechanics:
        risk += weather_bonus + 1 if weather_bonus >= 0 else weather_bonus - 1
    if card.card_type == CardType.BLESSING_CURSE:
        risk += 1.5 if own_tokens < opponent_tokens else -0.5
    if card.card_type == CardType.ARTIFACT:
        risk += 0.5
    if card.card_type == CardType.SUMMON and card.lifespan is not None:
        risk += max(0, 4 - card.lifespan) * 0.25
    hero_bonus = 2 if hero == card.element and hero == Element.FIRE else 3 if hero == card.element else 0
    morale = min(4.0, math.floor(max(0, own_tokens - opponent_tokens) / 2.0))
    return (
        card.power
        + risk
        + element_advantage(card.element, opponent.element, rule_sign)
        + hero_bonus
        + morale
        + synergy_bonus(card, hand, history, player)
    )


class RunenkriegEnv:
    """Deterministic parity-target environment used by every baseline."""

    def __init__(self, seed: int, rule_sign: float = 1.0):
        self.rng = random.Random(seed)
        self.rule_sign = rule_sign
        self.state = self._new_state()

    def _new_state(self) -> GameState:
        deck = build_deck(self.rng)
        return GameState(
            deck=deck[8:],
            player_hand=deck[:4],
            ai_hand=deck[4:8],
            player_hero=self.rng.choice((Element.FIRE, Element.MAGIC)),
            ai_hero=self.rng.choice((Element.FIRE, Element.MAGIC)),
        )

    def reset(self) -> GameState:
        self.state = self._new_state()
        return self.state

    def _refill(self, hand: list[Card], owner: str) -> None:
        while len(hand) < 4:
            if self.state.deck:
                hand.append(self.state.deck.pop())
            else:
                element = self.rng.choice(tuple(Element))
                power = self.rng.randrange(14)
                hand.append(
                    make_card(
                        f"{owner}-{len(self.state.history)}-{self.rng.random()}",
                        element,
                        power,
                    )
                )

    def opponent_action(self, policy: str = "mixed") -> Action:
        actions = legal_actions(self.state.player_hand)
        if policy == "random":
            return self.rng.choice(actions)
        if policy == "mixed":
            branch = self.rng.randrange(4)
            if branch == 0:
                return max(actions, key=lambda action: action.card.power)
            if branch == 1:
                return min(actions, key=lambda action: action.card.power)
            if branch == 2:
                fusion = [action for action in actions if len(action.consumed) == 2]
                return self.rng.choice(fusion or actions)
            return self.rng.choice(actions)
        weather = self.rng.choice(tuple(Weather))
        return max(
            actions,
            key=lambda action: max(
                combat_score(
                    action.card,
                    ai.card,
                    self.state.player_hero,
                    self.state.player_tokens,
                    self.state.ai_tokens,
                    weather,
                    [card for card in self.state.player_hand if card.uid not in action.consumed],
                    self.state.history,
                    True,
                    self.rule_sign,
                )
                for ai in legal_actions(self.state.ai_hand)
            ),
        )

    def round_context(self, opponent_policy: str = "mixed") -> tuple[Action, Weather, list[Action]]:
        player_action = self.opponent_action(opponent_policy)
        weather = self.rng.choice(tuple(Weather))
        return player_action, weather, legal_actions(self.state.ai_hand)

    def step(self, player_action: Action, ai_action: Action, weather: Weather) -> tuple[float, Winner]:
        state = self.state
        player_hand = [card for card in state.player_hand if card.uid not in player_action.consumed]
        ai_hand = [card for card in state.ai_hand if card.uid not in ai_action.consumed]
        player_score = combat_score(
            player_action.card, ai_action.card, state.player_hero,
            state.player_tokens, state.ai_tokens, weather, player_hand,
            state.history, True, self.rule_sign,
        )
        ai_score = combat_score(
            ai_action.card, player_action.card, state.ai_hero,
            state.ai_tokens, state.player_tokens, weather, ai_hand,
            state.history, False, self.rule_sign,
        )
        winner = Winner.PLAYER if player_score > ai_score else Winner.AI if ai_score > player_score else Winner.DRAW
        before_player, before_ai = state.player_tokens, state.ai_tokens
        state.player_tokens, state.ai_tokens = self._element_effect(
            winner,
            player_action.card if winner == Winner.PLAYER else ai_action.card if winner == Winner.AI else None,
        )
        self._mechanics(player_action.card, True, winner == Winner.PLAYER, player_hand, weather)
        self._mechanics(ai_action.card, False, winner == Winner.AI, ai_hand, weather)
        state.player_tokens = max(0, state.player_tokens)
        state.ai_tokens = max(0, state.ai_tokens)
        state.history.append(
            RoundRecord(
                player_action.card, ai_action.card, weather, winner,
                state.player_tokens, state.ai_tokens,
            )
        )
        state.player_hand, state.ai_hand = player_hand, ai_hand
        self._refill(state.player_hand, "player")
        self._refill(state.ai_hand, "ai")
        swing = (state.ai_tokens - before_ai) - (state.player_tokens - before_player)
        base = 0.9 if winner == Winner.AI else 0.5 if winner == Winner.DRAW else 0.1
        return max(0.0, min(1.0, base + max(-2, min(2, swing)) * 0.05)), winner

    def _element_effect(self, winner: Winner, card: Card | None) -> tuple[int, int]:
        player, ai = self.state.player_tokens, self.state.ai_tokens
        if card is None:
            return player, ai
        player_won = winner == Winner.PLAYER
        def heal(amount: int = 1) -> None:
            nonlocal player, ai
            if player_won: player += amount
            else: ai += amount
        def damage(amount: int = 1) -> None:
            nonlocal player, ai
            if player_won: ai -= amount
            else: player -= amount
        if card.element in (Element.FIRE, Element.ICE): damage()
        elif card.element == Element.WATER: heal(); damage()
        elif card.element in (Element.EARTH, Element.LIGHTNING): heal()
        elif card.element in (Element.AIR, Element.LIGHT): heal(2)
        elif card.element == Element.SHADOW: damage(); heal()
        elif card.element == Element.CHAOS:
            if (len(self.state.history) + 1) % 2 == 0: heal(); damage()
            else:
                if player_won: player -= 1; ai += 1
                else: ai -= 1; player += 1
        return max(0, player), max(0, ai)

    def _mechanics(self, card: Card, player_owned: bool, won: bool, hand: Sequence[Card], weather: Weather) -> None:
        state = self.state
        own = lambda: state.player_tokens if player_owned else state.ai_tokens
        opponent = lambda: state.ai_tokens if player_owned else state.player_tokens
        def set_own(value: int) -> None:
            if player_owned: state.player_tokens = value
            else: state.ai_tokens = value
        def set_opponent(value: int) -> None:
            if player_owned: state.ai_tokens = value
            else: state.player_tokens = value
        if Mechanic.CHAIN in card.mechanics and won and state.history:
            prior = state.history[-1]
            prior_card = prior.player_card if player_owned else prior.ai_card
            prior_win = prior.winner == (Winner.PLAYER if player_owned else Winner.AI)
            if Mechanic.CHAIN in prior_card.mechanics and prior_win:
                set_opponent(max(0, opponent() - 1))
        if Mechanic.RESONANCE in card.mechanics and won:
            count = 1 + sum(
                (record.player_card if player_owned else record.ai_card).element == card.element
                for record in state.history
            )
            if count >= 3: set_own(own() + 1)
        if Mechanic.OVERLOAD in card.mechanics: set_own(max(0, own() - 1))
        if Mechanic.WEATHER_BOND in card.mechanics:
            set_own(max(0, own() + int(weather_modifier(weather, card.element))))
        if Mechanic.ALLY in card.mechanics and any(candidate.element == card.element for candidate in hand):
            set_own(own() + 1)
        if Mechanic.BLESSING_CURSE in card.mechanics:
            if own() < opponent(): set_own(own() + 1)
            else: set_opponent(max(0, opponent() - 1))
