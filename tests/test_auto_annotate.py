"""Synthetic ground-truth tests for the S1/S2 annotator's onset decisions."""

import unittest

import numpy as np

import auto_annotate as aa


def synthetic_hf_envelope(*, sr=aa.SR, level=1.0, onset_ms=60.0, peak_ms=80.0,
                          decay_ms=25.0, duration_ms=160.0):
    """Known-onset transient on a nonzero noise floor."""
    t_ms = np.arange(int(duration_ms * 1e-3 * sr)) * 1e3 / sr
    floor = np.full_like(t_ms, 0.01)
    rise = np.clip((t_ms - onset_ms) / (peak_ms - onset_ms), 0.0, 1.0)
    attack = rise * rise * (3.0 - 2.0 * rise)
    tail = np.exp(-np.maximum(t_ms - peak_ms, 0.0) / decay_ms)
    transient = np.where(t_ms <= peak_ms, attack, tail)
    return level * (floor + transient)


def smoothstep_threshold_ms(*, support_ms=60.0, peak_ms=80.0, frac=aa.S2_FRAC):
    """Analytic time where 3u^2-2u^3 reaches the detector's floor-relative fraction."""
    roots = np.roots((-2.0, 3.0, 0.0, -frac))
    u = next(float(root.real) for root in roots if abs(root.imag) < 1e-12 and 0.0 <= root.real <= 1.0)
    return support_ms + u * (peak_ms - support_ms)


class S2OnsetTests(unittest.TestCase):
    def test_floor_crossing_distinguishes_a_real_crossing_from_valley_fallback(self):
        envelope = np.ones(200)
        envelope[100:] = np.linspace(1.0, 10.0, 100)
        self.assertTrue(aa._crossed_floor(envelope, 199, 1.0, 0.1, 50))

        fused = envelope + 1.1  # entire pre-peak walk stays above the threshold
        self.assertFalse(aa._crossed_floor(fused, 199, 1.0, 0.1, 50))

    def test_hf_fallback_recovers_known_onset_and_is_level_invariant(self):
        sr = aa.SR
        expected = int(smoothstep_threshold_ms() * 1e-3 * sr)
        s2_peak = int(0.090 * sr)
        valley = int(0.020 * sr)
        measured = []
        for level in (0.1, 1.0, 10.0):
            envelope = synthetic_hf_envelope(level=level)
            onset = aa._hf_fallback_onset(envelope, s2_peak, valley, len(envelope) - 1, sr)
            self.assertIsNotNone(onset)
            measured.append(onset)
            self.assertAlmostEqual(onset, expected, delta=2)
        self.assertEqual(len(set(measured)), 1)

    def test_hf_fallback_is_invariant_to_decay_tail(self):
        sr = aa.SR
        expected = int(smoothstep_threshold_ms() * 1e-3 * sr)
        onsets = []
        for decay_ms in (8.0, 25.0, 70.0):
            envelope = synthetic_hf_envelope(decay_ms=decay_ms)
            onsets.append(aa._hf_fallback_onset(
                envelope, int(0.090 * sr), int(0.020 * sr), len(envelope) - 1, sr))
        for onset in onsets:
            self.assertIsNotNone(onset)
            self.assertAlmostEqual(onset, expected, delta=2)

    def test_hf_fallback_rejects_a_valley_already_on_the_same_rise(self):
        sr = aa.SR
        envelope = synthetic_hf_envelope()
        onset = aa._hf_fallback_onset(
            envelope, int(0.090 * sr), int(0.050 * sr), len(envelope) - 1, sr)
        self.assertIsNone(onset)  # known onset is only 10ms after the valley, below the 20ms guard

    def test_hf_fallback_rejects_low_snr_activity(self):
        sr = aa.SR
        envelope = synthetic_hf_envelope()
        envelope = 0.9 + 0.1 * envelope              # peak/background well below S2_HF_SNR_MIN
        onset = aa._hf_fallback_onset(
            envelope, int(0.090 * sr), int(0.020 * sr), len(envelope) - 1, sr)
        self.assertIsNone(onset)


if __name__ == "__main__":
    unittest.main()
