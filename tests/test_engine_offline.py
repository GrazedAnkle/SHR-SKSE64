import sys
import unittest
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import engine_offline as eo
from shrlib import SR


class LegacyTailAuditionControlTests(unittest.TestCase):
    def setUp(self):
        t = np.arange(eo.S1_END - eo.S1_ON) / SR
        envelope = np.exp(-t / 0.060)
        self.source = 12000.0 * envelope * np.sin(2.0 * np.pi * 60.0 * t + 0.3)

    def test_default_tail_mode_is_the_shipping_no_op(self):
        self.assertIs(eo.apply_tail(self.source), self.source)

    def test_off_mode_is_an_exact_no_op(self):
        out = eo.apply_tail(self.source, "off")
        self.assertIs(out, self.source)

    def test_dry_end_mode_matches_fixed_at_the_uncompressed_source_length(self):
        fixed = eo.tail_splice_frame(len(self.source), "fixed")
        relative = eo.tail_splice_frame(len(self.source), "dry-end")
        self.assertEqual(relative, fixed)

    def test_dry_end_mode_preserves_the_handoff_when_the_source_shortens(self):
        shortened = self.source[:-int(6.0e-3 * SR)]
        fixed = eo.tail_splice_frame(len(shortened), "fixed")
        relative = eo.tail_splice_frame(len(shortened), "dry-end")
        ramp = int(eo.LEGACY_TAIL_RAMP_MS * 1.0e-3 * SR)

        self.assertGreater(fixed + ramp, len(shortened))
        self.assertEqual(len(shortened) - (relative + ramp), int(2.0e-3 * SR))

    def test_unknown_tail_mode_is_rejected(self):
        with self.assertRaises(ValueError):
            eo.apply_tail(self.source, "mystery")

    def test_synth_default_is_explicit_no_tamer_no_tail(self):
        s2 = np.zeros(eo.S2_END - eo.S2_ON)
        default = eo.synth_beat(self.source, s2, 180.0, 1.0, 1.0, breath_depth=0.0)
        explicit = eo.synth_beat(
            self.source, s2, 180.0, 1.0, 1.0,
            breath_depth=0.0, tail_mode="off", tamer_enabled=False
        )

        np.testing.assert_array_equal(default, explicit)


if __name__ == "__main__":
    unittest.main()
