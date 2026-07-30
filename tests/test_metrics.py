"""Synthetic ground-truth tests for the audio measurement rulers."""

import unittest

import numpy as np

import shrlib


SR = shrlib.SR


def synthetic_lobe(f0_hz=80.0, rise_ms=40.0, level=1.0, null=False, lobes=1,
                   noise_rms=0.0, seed=0):
    """One deterministic S1-like lobe with known timing and optional white-noise floor.

    `noise_rms` is measured before `level`, so scaling the fixture preserves SNR. Tests that need a
    fixed absolute recording floor can add noise after this function returns.
    """
    t = np.arange(int(0.18 * SR)) / SR
    rise = np.clip((t - 0.020) / (rise_ms * 1e-3), 0.0, 1.0)
    envelope = rise * rise * (3.0 - 2.0 * rise) * np.exp(-np.maximum(t - 0.020 - rise_ms * 1e-3, 0.0) / 0.045)
    if null:
        envelope *= 1.0 - 0.99 * np.exp(-0.5 * ((t - 0.045) / 0.0015) ** 2)
    carrier = np.sin(2.0 * np.pi * f0_hz * t)
    signal = envelope * carrier
    if lobes > 1:
        signal += 0.72 * np.roll(signal, int(0.022 * SR))
    if noise_rms > 0.0:
        noise = np.random.default_rng(seed).standard_normal(len(signal))
        noise *= noise_rms / np.sqrt(np.mean(noise ** 2))
        signal += noise
    return level * signal


def synthetic_hf_geometry(duration_ms=140.0, window_fraction=1.0):
    """Two-lobe energy geometry whose sign flips when the second lobe is truncated.

    This tests the temporal-centroid ruler directly rather than pretending the fixture is a heart
    recording. Broadband energy has an early light lobe and a late heavy lobe; HF energy lies between
    them. Over complete support HF leads the broadband centroid. A short window removes the late
    broadband lobe and makes the same HF energy appear to trail.
    """
    support = max(2, int(round(duration_ms * 1e-3 * SR)))
    width = max(2, int(round(support * window_fraction)))
    unit_time = np.arange(support) / support

    def gaussian(center, sigma):
        return np.exp(-0.5 * ((unit_time - center) / sigma) ** 2)

    broadband_energy = gaussian(0.18, 0.045) + 3.0 * gaussian(0.80, 0.055)
    hf_energy = gaussian(0.30, 0.040)
    lobe, hf = np.sqrt(broadband_energy), np.sqrt(hf_energy)
    if width < support:
        return lobe[:width], hf[:width]
    return np.pad(lobe, (0, width - support)), np.pad(hf, (0, width - support))


class MetricAuditTests(unittest.TestCase):
    def test_metadata_covers_the_metric_battery(self):
        expected = {
            "env", "env_analytic", "onset_peak_idx", "attack_ms", "rise_10_90_ms",
            "decay_ms", "centroid", "rolloff", "spread", "f0", "bands", "rms",
            "peak", "crest", "chirp", "energy_conc", "hf_band", "lobe_count",
            "hf_temporal_skew", "rise_body_contrast", "a_weight",
        }
        self.assertEqual(set(shrlib.METRIC_METADATA), expected)
        for metadata in shrlib.METRIC_METADATA.values():
            self.assertIn(metadata["anchor"], {"none", "peak", "threshold", "annotation"})
            self.assertIn(metadata["valid"], {"within-signal", "cross-signal"})
        skew = shrlib.METRIC_METADATA["hf_temporal_skew"]
        self.assertEqual(skew["role"], "hf-lead-lag-diagnostic")
        self.assertEqual(skew["window"], "complete-s1-support")
        self.assertEqual(skew["aggregation"], "group-median-for-references")
        self.assertEqual(set(skew["invalid"]), {"truncated-s1", "material-nonlinear-distortion"})
        self.assertEqual(
            set(shrlib.METRIC_METADATA["rise_10_90_ms"]["invalid"]),
            {"pre-onset-component-near-10pct"},
        )

    def test_rise_10_90_is_invariant_to_f0_and_level(self):
        reference = shrlib.rise_10_90_ms(synthetic_lobe(), SR)
        for f0_hz in (40.0, 120.0, 200.0):
            for level in (0.1, 1.0, 10.0):
                measured = shrlib.rise_10_90_ms(synthetic_lobe(f0_hz, level=level), SR)
                self.assertAlmostEqual(measured, reference, delta=1.0)

    def test_rise_10_90_matches_the_smoothstep_ground_truth(self):
        # smoothstep reaches 10% and 90% at x=0.1958 and x=0.8042: 60.84% of the declared rise.
        expected_ms = 40.0 * (0.8042 - 0.1958)
        self.assertAlmostEqual(shrlib.rise_10_90_ms(synthetic_lobe(), SR), expected_ms, delta=0.15)

    def test_rise_and_f0_survive_a_20_db_white_noise_floor(self):
        """Pins the useful domain. This does not assert noise immunity at arbitrary SNR.

        The noise RMS is one tenth of the clean lobe RMS (20 dB SNR). This is deliberately a timing/F0
        contract only: broadband centroid and lobe count legitimately respond to the added floor.
        """
        for f0_hz in (40.0, 80.0, 140.0, 200.0):
            clean = synthetic_lobe(f0_hz)
            noise_rms = shrlib.rms(clean) * 0.1
            noisy = synthetic_lobe(f0_hz, noise_rms=noise_rms)
            self.assertAlmostEqual(
                shrlib.rise_10_90_ms(noisy, SR), shrlib.rise_10_90_ms(clean, SR), delta=1.1)
            self.assertAlmostEqual(shrlib.f0(noisy, SR, hi=220.0), f0_hz, delta=0.5)

    def test_rise_10_90_is_immune_to_a_pre_peak_null(self):
        plain = shrlib.rise_10_90_ms(synthetic_lobe(), SR)
        nulled = shrlib.rise_10_90_ms(synthetic_lobe(null=True), SR)
        self.assertAlmostEqual(nulled, plain, delta=1.0)

    def test_rise_10_90_is_invalidated_by_a_precursor_near_its_threshold(self):
        """A tiny precursor change can move the relative-threshold anchor discontinuously.

        The main lobe and its ground-truth rise are identical. Only an earlier component moves from
        just below to just above 10% of the main peak, making the ruler start on another component.
        This pins an invalid comparison domain rather than blessing either reported value.
        """
        t = np.arange(int(0.18 * SR)) / SR
        main = np.exp(-0.5 * ((t - 0.095) / 0.018) ** 2)

        def with_precursor(level: float, phase: float) -> np.ndarray:
            precursor = level * np.exp(-0.5 * ((t - 0.032) / 0.006) ** 2)
            return (main + precursor) * np.sin(2.0 * np.pi * 200.0 * t + phase)

        for phase in (0.0, 0.7, 1.4, 2.2):
            below = shrlib.rise_10_90_ms(with_precursor(0.095, phase), SR)
            above = shrlib.rise_10_90_ms(with_precursor(0.100, phase), SR)
            self.assertGreater(above - below, 20.0)

    def test_attack_ms_exposes_the_known_null_sensitive_ruler(self):
        plain = shrlib.attack_ms(synthetic_lobe(), SR)
        nulled = shrlib.attack_ms(synthetic_lobe(null=True), SR)
        self.assertGreater(abs(nulled - plain), 2.0)

    def test_f0_is_invariant_to_level(self):
        for f0_hz in (40.0, 80.0, 140.0, 200.0):
            tone = np.sin(2.0 * np.pi * f0_hz * np.arange(SR) / SR)
            self.assertAlmostEqual(shrlib.f0(tone, SR, hi=220.0), f0_hz, delta=0.5)
            self.assertAlmostEqual(shrlib.f0(0.01 * tone, SR, hi=220.0), f0_hz, delta=0.5)

    def test_energy_concentration_has_known_window_partition(self):
        signal = np.ones(int(0.08 * SR))
        e = shrlib.energy_conc(signal, SR)
        self.assertEqual(len(e), 3)
        self.assertAlmostEqual(sum(e), 1.0, places=6)
        self.assertAlmostEqual(e[0], 0.25, places=6)
        self.assertAlmostEqual(e[1], 0.25, places=6)
        self.assertAlmostEqual(e[2], 0.50, places=6)
        scaled = shrlib.energy_conc(7.0 * signal, SR)
        np.testing.assert_allclose(scaled, e, atol=1e-12)

    def test_a_weight_gain_matches_the_iec_61672_curve(self):
        for hz, expected_db in ((1000.0, 0.0), (100.0, -19.1), (50.0, -30.2), (31.5, -39.4)):
            gain_db = 20.0 * np.log10(shrlib.a_weight_gain(np.array([hz]))[0])
            self.assertAlmostEqual(gain_db, expected_db, delta=0.2)

    def test_a_weighted_level_is_stable_across_window_lengths(self):
        """The whole-signal filter is what makes a windowed A-weighted RMS mean anything.

        A-weighting has a ~30-50 ms impulse-response tail (its 20.6 Hz pole pair), so it must be applied
        to the signal and THEN windowed. Filter a steady tone, cut windows of different lengths out of the
        middle, and the level must not care how wide the window is.
        """
        tone = np.sin(2.0 * np.pi * 80.0 * np.arange(int(0.5 * SR)) / SR)
        weighted = shrlib.a_weight(tone, SR)
        mid = len(tone) // 2
        levels = [shrlib.rms(weighted[mid:mid + int(ms * 1e-3 * SR)]) for ms in (55, 65, 75, 95)]
        for level in levels[1:]:
            self.assertAlmostEqual(20.0 * np.log10(level / levels[0]), 0.0, delta=0.15)

    def test_a_weighting_a_short_window_in_isolation_is_the_trap(self):
        """Multiplying a short window's rfft by the gain curve and inverse-transforming is a CIRCULAR
        convolution: the filter's tail wraps onto the window's start. The trigger is SUB-AUDIBLE LF -
        exactly what A-weighting exists to reject. The wraparound smears rumble across the window instead
        of rejecting it, so the level reads high. A clean periodic signal does not expose this failure.
        """
        t = np.arange(int(0.9 * SR)) / SR
        signal = 0.5 * np.sin(2.0 * np.pi * 8.0 * t)                       # sub-audible rumble
        lobe = synthetic_lobe()
        signal[int(0.2 * SR):int(0.2 * SR) + len(lobe)] += lobe
        start, width = int(0.2 * SR), int(0.075 * SR)

        window = signal[start:start + width]
        spectrum = np.fft.rfft(window)
        freq = np.fft.rfftfreq(len(window), 1.0 / SR)
        circular = shrlib.rms(np.fft.irfft(spectrum * shrlib.a_weight_gain(freq), n=len(window)))
        correct = shrlib.rms(shrlib.a_weight(signal, SR)[start:start + width])
        self.assertGreater(20.0 * np.log10(circular / correct), 0.5)

    def test_level_invariant_shape_metrics(self):
        signal = synthetic_lobe()
        for metric in (shrlib.centroid, shrlib.rolloff, shrlib.spread):
            self.assertAlmostEqual(metric(signal, SR), metric(9.0 * signal, SR), delta=1e-9)
        self.assertAlmostEqual(shrlib.crest(signal), shrlib.crest(9.0 * signal), delta=1e-9)
        np.testing.assert_allclose(shrlib.bands(signal, SR), shrlib.bands(9.0 * signal, SR), atol=1e-12)

    def test_hf_temporal_skew_has_known_anchor_free_value(self):
        lobe = np.zeros(100)
        hf = np.zeros(100)
        lobe[[20, 80]] = 1.0       # broadband energy centroid = 0.50 of the window
        hf[20] = 1.0               # HF energy centroid = 0.20 of the window
        expected = -0.30
        self.assertAlmostEqual(shrlib.hf_temporal_skew(lobe, hf), expected, places=12)
        self.assertAlmostEqual(shrlib.hf_temporal_skew(7.0 * lobe, 0.2 * hf), expected, places=12)

    def test_hf_temporal_skew_tracks_duration_only_with_matched_full_support(self):
        """Time-scaled S1s agree when the window is the same fraction of actual S1 duration."""
        durations_ms = (80.0, 100.0, 130.0, 160.0, 200.0)
        for window_fraction in (1.0, 1.10, 1.25):
            measured = [
                shrlib.hf_temporal_skew(*synthetic_hf_geometry(duration, window_fraction))
                for duration in durations_ms
            ]
            self.assertLess(max(measured) - min(measured), 2e-4)

        full = shrlib.hf_temporal_skew(*synthetic_hf_geometry(140.0, 1.0))
        padded = shrlib.hf_temporal_skew(*synthetic_hf_geometry(140.0, 1.25))
        # Trailing zero padding cannot move either physical centroid, but the ruler divides their
        # distance by the declared window. Even benign padding therefore requires a matched W/D_S1.
        self.assertAlmostEqual(padded, full / 1.25, delta=2e-4)

    def test_hf_temporal_skew_rejects_a_truncated_two_lobe_s1(self):
        """A duration-matched window is not optional: truncation can reverse the interpretation."""
        for duration_ms in (80.0, 100.0, 130.0, 160.0, 200.0):
            full = shrlib.hf_temporal_skew(*synthetic_hf_geometry(duration_ms, 1.0))
            truncated = shrlib.hf_temporal_skew(*synthetic_hf_geometry(duration_ms, 0.60))
            self.assertLess(full, -0.30)
            self.assertGreater(truncated, 0.15)

    def test_hf_temporal_skew_is_invalidated_by_capture_saturation(self):
        """Nonlinear capture manufactures HF where the waveform hits its ceiling.

        The underlying two-lobe source and its HF lead are unchanged. Hard saturation alone moves the
        measured result from a strong lead to a lag, across carrier start phases. This is an executable
        invalid-domain result, not a proposed generic crest-factor gate.
        """
        duration = int(0.140 * SR)
        padding = int(0.100 * SR)  # keep the whole-signal bandpass away from file edges
        t = np.arange(duration) / SR
        unit_time = np.arange(duration) / duration

        def gaussian(center, sigma):
            return np.exp(-0.5 * ((unit_time - center) / sigma) ** 2)

        for phase in (0.0, 0.3, 0.7, 1.4, 2.2):
            low = (
                0.45 * gaussian(0.22, 0.08) + gaussian(0.68, 0.13)
            ) * np.sin(2.0 * np.pi * 60.0 * t + phase)
            early_hf = 0.12 * gaussian(0.24, 0.05) * np.sin(
                2.0 * np.pi * 320.0 * t + 0.7 + phase)
            source = low + early_hf
            source /= np.max(np.abs(source))
            clean = np.pad(source, (padding, padding))
            saturated = np.clip(2.8 * clean, -0.42, 0.42)

            clean_hf = shrlib.hf_band(clean, SR)[padding:padding + duration]
            saturated_hf = shrlib.hf_band(saturated, SR)[padding:padding + duration]
            clean_skew = shrlib.hf_temporal_skew(source, clean_hf)
            saturated_skew = shrlib.hf_temporal_skew(
                saturated[padding:padding + duration], saturated_hf)

            self.assertLess(clean_skew, -0.30)
            self.assertGreater(saturated_skew, 0.0)
            self.assertGreater(saturated_skew - clean_skew, 0.35)


if __name__ == "__main__":
    unittest.main()
