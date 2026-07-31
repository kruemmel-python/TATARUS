from __future__ import annotations

import unittest

from runenkrieg_tf.analyze_results import (
    exact_paired_permutation,
    holm_adjust,
)
from runenkrieg_tf.select_winner import bootstrap_interval


class StatisticsTest(unittest.TestCase):
    def test_identical_models_have_permutation_p_one(self) -> None:
        self.assertEqual(exact_paired_permutation([0.0] * 5), 1.0)

    def test_consistent_direction_has_exact_five_seed_resolution(self) -> None:
        self.assertEqual(
            exact_paired_permutation([0.2, 0.1, 0.3, 0.2, 0.4]),
            0.0625,
        )

    def test_holm_adjustment_is_monotonic(self) -> None:
        adjusted = holm_adjust([("a", 0.01), ("b", 0.03), ("c", 0.5)])
        self.assertAlmostEqual(adjusted["a"], 0.03)
        self.assertAlmostEqual(adjusted["b"], 0.06)
        self.assertAlmostEqual(adjusted["c"], 0.5)

    def test_bootstrap_single_value_is_degenerate(self) -> None:
        self.assertEqual(bootstrap_interval([0.75]), (0.75, 0.75))


if __name__ == "__main__":
    unittest.main()
