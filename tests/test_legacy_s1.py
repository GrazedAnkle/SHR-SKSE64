import unittest

import numpy as np

import legacy_s1
from shrlib import SR


class LegacyTailCounterfactualTests(unittest.TestCase):
    def setUp(self):
        self.baseline_frames = int(95.0e-3 * SR)
        time = np.arange(self.baseline_frames) / SR
        envelope = np.exp(-time / 0.060)
        mono = 0.35 * envelope * np.sin(2.0 * np.pi * 60.0 * time + 0.3)
        self.source = np.column_stack([mono, mono])

    def test_default_tail_mode_is_an_exact_no_op(self):
        self.assertIs(legacy_s1.apply_tail(self.source), self.source)

    def test_dry_end_mode_matches_fixed_at_the_uncompressed_source_length(self):
        fixed = legacy_s1.tail_splice_frame(
            len(self.source),
            self.baseline_frames,
            "fixed",
        )
        relative = legacy_s1.tail_splice_frame(
            len(self.source),
            self.baseline_frames,
            "dry-end",
        )
        self.assertEqual(relative, fixed)

    def test_dry_end_mode_preserves_the_handoff_when_the_source_shortens(self):
        shortened = self.source[:-int(6.0e-3 * SR)]
        fixed = legacy_s1.tail_splice_frame(
            len(shortened),
            self.baseline_frames,
            "fixed",
        )
        relative = legacy_s1.tail_splice_frame(
            len(shortened),
            self.baseline_frames,
            "dry-end",
        )
        ramp = int(legacy_s1.LEGACY_TAIL_RAMP_MS * 1.0e-3 * SR)

        self.assertGreater(fixed + ramp, len(shortened))
        self.assertEqual(len(shortened) - (relative + ramp), int(2.0e-3 * SR))

    def test_tail_preserves_native_channel_layout(self):
        output = legacy_s1.apply_tail(
            self.source,
            "fixed",
            baseline_dry_frames=self.baseline_frames,
        )
        self.assertEqual(output.ndim, 2)
        self.assertEqual(output.shape[1], 2)
        self.assertGreater(len(output), len(self.source))
        np.testing.assert_array_equal(output[:, 0], output[:, 1])

    def test_tamer_preserves_native_channel_layout(self):
        output = legacy_s1.tame_lobe(self.source, 1.0)
        self.assertEqual(output.shape, self.source.shape)
        np.testing.assert_array_equal(output[:, 0], output[:, 1])

    def test_unknown_tail_mode_is_rejected(self):
        with self.assertRaises(ValueError):
            legacy_s1.apply_tail(self.source, "mystery")


if __name__ == "__main__":
    unittest.main()
