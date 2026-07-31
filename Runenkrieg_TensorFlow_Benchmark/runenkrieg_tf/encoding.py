from __future__ import annotations

import numpy as np

from .environment import Action, Card, Element, GameState, Mechanic, Weather, Winner


def _streak(state: GameState, winner: Winner) -> float:
    count = 0
    for record in reversed(state.history):
        if record.winner != winner:
            break
        count += 1
    return min(1.0, count / 4.0)


def encode(state: GameState, player_card: Card, action: Action, weather: Weather) -> np.ndarray:
    """The same 128-channel candidate space used by TATARUS LargeScale."""
    card = action.card
    x = np.zeros(128, dtype=np.float32)
    x[card.element.value] = 1.0
    x[10] = card.power / 13.0
    x[11] = card.card_type.value / 4.0
    x[12] = float(card.fused)
    x[13] = len(card.mechanics) / 7.0
    x[14] = 0.0  # no precomputed element advantage
    x[15] = 0.0  # no precomputed weather advantage
    x[16] = np.clip(state.ai_tokens / 12.0, 0, 1)
    x[17] = np.clip(state.player_tokens / 12.0, 0, 1)
    x[18] = float(state.ai_hero == card.element)
    for mechanic in Mechanic:
        x[19 + mechanic.value] = float(mechanic in card.mechanics)
    x[26] = float(card.card_type.value == 0)
    x[27] = float(card.card_type.value == 1)
    x[28] = 0.0
    x[29] = len(action.consumed) / 2.0
    x[30] = sum(record.ai_card.element == card.element for record in state.history[-4:]) / 4.0
    x[31] = 1.0
    x[32 + card.power] = 1.0
    x[46 + card.card_type.value] = 1.0
    for mechanic in Mechanic:
        x[51 + mechanic.value] = float(mechanic in card.mechanics)
    x[58 + player_card.element.value] = 1.0
    x[68 + player_card.element.value] = card.power / 13.0
    x[78 + card.element.value] = (weather.value + 1) / 3.0
    for element in Element:
        x[88 + element.value] = sum(c.element == element for c in state.ai_hand) / max(1, len(state.ai_hand))
        x[98 + element.value] = sum(r.ai_card.element == element for r in state.history[-6:]) / 6.0
        x[108 + element.value] = sum(r.player_card.element == element for r in state.history[-6:]) / 6.0
    x[118] = (card.power / 13.0) ** 2
    x[119] = np.clip((state.ai_tokens - state.player_tokens + 10) / 20.0, 0, 1)
    x[120] = float(state.ai_tokens <= 2)
    x[121] = float(card.power >= 12)
    x[122] = np.clip((len(state.history) + 1) / 100.0, 0, 1)
    x[123] = sum(
        r.ai_card.element == card.element and r.ai_card.power == card.power
        for r in state.history[-6:]
    ) / 6.0
    x[124] = sum(c.element == card.element for c in state.ai_hand) / max(1, len(state.ai_hand))
    x[125] = np.clip((len(state.ai_hand) - len(action.consumed)) / 4.0, 0, 1)
    x[126] = sum(r.winner == Winner.AI for r in state.history[-4:]) / 4.0
    x[127] = 1.0
    return x

