import random
import unittest

import numpy as np

from runenkrieg_tf.encoding import encode
from runenkrieg_tf.environment import RunenkriegEnv, legal_actions


class EnvironmentTest(unittest.TestCase):
    def test_same_seed_produces_same_initial_game(self):
        first = RunenkriegEnv(73).state
        second = RunenkriegEnv(73).state
        self.assertEqual(first.player_hand, second.player_hand)
        self.assertEqual(first.ai_hand, second.ai_hand)
        self.assertEqual(first.player_hero, second.player_hero)
        self.assertEqual(first.ai_hero, second.ai_hero)

    def test_encoder_has_exactly_128_finite_channels(self):
        env = RunenkriegEnv(91)
        player, weather, actions = env.round_context()
        vector = encode(env.state, player.card, actions[0], weather)
        self.assertEqual((128,), vector.shape)
        self.assertEqual(np.float32, vector.dtype)
        self.assertTrue(np.isfinite(vector).all())
        self.assertEqual(1.0, vector[127])

    def test_round_changes_history_and_returns_bounded_reward(self):
        env = RunenkriegEnv(107)
        player, weather, actions = env.round_context()
        reward, _ = env.step(player, actions[0], weather)
        self.assertEqual(1, len(env.state.history))
        self.assertGreaterEqual(reward, 0.0)
        self.assertLessEqual(reward, 1.0)
        self.assertEqual(4, len(env.state.player_hand))
        self.assertEqual(4, len(env.state.ai_hand))

    def test_fusion_actions_are_exposed_when_available(self):
        for seed in range(100):
            env = RunenkriegEnv(seed)
            actions = legal_actions(env.state.ai_hand)
            if any(len(action.consumed) == 2 for action in actions):
                self.assertTrue(any(action.card.fused for action in actions))
                return
        self.fail("No deterministic seed exposed a fusion action.")


if __name__ == "__main__":
    unittest.main()

