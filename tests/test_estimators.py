"""Synthetic ground-truth tests for the COMPOSITE estimators in tools/rhythm_offline.py.

tests/test_metrics.py audits the shrlib PRIMITIVES. But the rulers that lied most recently did not live
there - they lived one layer up, in rhythm_offline's breath-swing estimator and its window rules. This
file brings that layer into the audited surface.

Two things make a synthetic ground truth possible without rendering any audio:

  - the breath swing is a pure statistic of (per-beat value, per-beat inflation), so it can be driven by
    a sampling process whose answer is known in closed form;
  - lung inflation is sin(pi*phase) with the phase advancing a fixed step per beat, so its limit
    distribution - and hence the estimator's fixed point - is exactly computable.

That lets the tests assert ACCURACY against a known answer, not merely stability, which is the stronger
claim and the one that catches a ruler locked onto a wrong value.
"""

import unittest
from pathlib import Path

import numpy as np

import shrlib
import rhythm_offline as ro
import core_offline

ROOT = Path(__file__).resolve().parents[1]


SR = shrlib.SR
SOURCE = ROOT / "contrib/Distribution/Sound/fx/SHR_HeartBeat/HeartBeat_Shortened.wav"

# A monotone-decreasing stand-in for the breath muffle: the more inflated the lung, the quieter and duller
# the beat. Its exact shape does not matter - only that it is monotone, which is what makes the estimator's
# limit exactly computable (a median commutes with a monotone transform, so the true swing is just the
# transform evaluated at the limit inflation-group medians).
def muffle(inflation):
    return (1.0 - 0.47 * inflation) * np.exp(-0.55 * inflation)


def beat_sampled_inflations(beats_per_breath, n_beats):
    """The respiratory phases a rigid rotation actually visits - the engine's sampling process at
    exertion 1, where RSA is off and each beat advances the phase by exactly resp_rate/hr."""
    phases = (np.arange(n_beats) / beats_per_breath) % 1.0
    return np.sin(np.pi * phases)


def true_swing_db():
    """The swing a beat run with perfect phase coverage must report (on the ENGINE's curve)."""
    exp_median, insp_median = ro.breath_group_median_targets(shrlib.SINE)
    return 20.0 * np.log10(muffle(insp_median) / muffle(exp_median))


def dense_phase(n=200_001):
    return np.linspace(0.0, 1.0, n, endpoint=False)


class BreathGroupTargetTests(unittest.TestCase):
    """The closed-form fixed point the coverage guard is built on."""

    def test_closed_form_targets_match_a_dense_empirical_sample(self):
        dense = np.sin(np.pi * dense_phase())
        for group_frac in (0.20, 0.30, 0.50):
            expiration, inspiration = ro.breath_groups(dense, group_frac)
            empirical = (np.median(dense[expiration]), np.median(dense[inspiration]))
            predicted = ro.breath_group_median_targets(shrlib.SINE, group_frac)
            for measured, expected in zip(empirical, predicted):
                self.assertAlmostEqual(measured, expected, delta=1e-3)

    def test_swing_is_exact_under_uniform_phase_coverage(self):
        dense = np.sin(np.pi * dense_phase())
        measured = ro.breath_swing_db(muffle(dense), dense)
        self.assertAlmostEqual(measured, true_swing_db(), delta=0.01)


class InflationCurveTests(unittest.TestCase):
    """Engine and reference inflation curves have distinct coverage targets; callers must name one."""

    def test_each_curve_targets_match_ITS_OWN_dense_sample(self):
        for curve, dense in ((shrlib.SINE, np.sin(np.pi * dense_phase())),
                             (shrlib.RAISED_COSINE,
                              0.5 * (1.0 - np.cos(2.0 * np.pi * dense_phase())))):
            expiration, inspiration = ro.breath_groups(dense)
            empirical = (np.median(dense[expiration]), np.median(dense[inspiration]))
            for measured, expected in zip(empirical, shrlib.breath_group_median_targets(curve)):
                self.assertAlmostEqual(measured, expected, delta=1e-3, msg=f"curve {curve}")

    def test_the_two_curves_have_DIFFERENT_targets(self):
        """The bug is only possible because these differ - so assert the gap, at the value that caused it."""
        sine = shrlib.breath_group_median_targets(shrlib.SINE)
        raised = shrlib.breath_group_median_targets(shrlib.RAISED_COSINE)
        self.assertAlmostEqual(sine[0], 0.2334, delta=1e-3)
        self.assertAlmostEqual(raised[0], 0.0545, delta=1e-3)
        # The target-family gap is large enough to manufacture a coverage failure.
        self.assertAlmostEqual(sine[0] - raised[0], 0.179, delta=1e-3)

    def test_grading_a_reference_against_the_ENGINE_targets_manufactures_a_failure(self):
        """The bug itself, reproduced: a PERFECTLY covered raised-cosine run is judged under-covered when
        it is graded on the sine targets, and passes on its own."""
        dense = 0.5 * (1.0 - np.cos(2.0 * np.pi * dense_phase(4001)))
        expiration, inspiration = ro.breath_groups(dense)
        realized = (np.median(dense[expiration]), np.median(dense[inspiration]))

        def deviation(curve):
            targets = shrlib.breath_group_median_targets(curve)
            return max(abs(r - t) for r, t in zip(realized, targets))

        self.assertLess(deviation(shrlib.RAISED_COSINE), shrlib.BREATH_MEDIAN_TOL)
        self.assertGreater(deviation(shrlib.SINE), 8 * shrlib.BREATH_MEDIAN_TOL)

    def test_the_curve_must_be_named(self):
        with self.assertRaises(ValueError):
            shrlib.breath_group_median_targets("half-sine")


class ShortGroupBiasTests(unittest.TestCase):
    """A reference group is a SEMANTIC marker, but every reference figure is a median over one,
    and the breath modulates the metrics being median'd. `breath_group_bias` puts an error bar on that.
    """

    def test_bias_vanishes_when_the_group_spans_a_WHOLE_number_of_breaths(self):
        for n_beats, ratio in ((8, 8.0), (8, 4.0), (12, 6.0), (12, 4.0), (20, 10.0)):
            sd, _ = shrlib.breath_group_bias(n_beats, ratio, shrlib.RAISED_COSINE)
            self.assertLess(sd, 0.01, f"{n_beats} beats at {ratio}/breath spans "
                                      f"{n_beats / ratio:.0f} whole breaths - it must be unbiased")

    def test_bias_is_WORST_when_the_group_spans_a_fraction_of_a_breath(self):
        """The controlling quantity is the SPAN, not the beat count: a group covering half a cycle is
        maximally biased however many beats it has."""
        whole, _ = shrlib.breath_group_bias(8, 8.0, shrlib.RAISED_COSINE)      # spans 1.00 breaths
        fraction, _ = shrlib.breath_group_bias(8, 16.0, shrlib.RAISED_COSINE)  # spans 0.50 breaths
        self.assertGreater(fraction, 10 * max(whole, 1e-6))
        self.assertGreater(fraction, 0.2)   # >= 20% of the metric's whole breath swing

    def test_ref20s_measured_group_is_predicted_to_be_nearly_unbiased(self):
        """The one case with real beats behind it: ref20's 8-beat groups sit at 8.47 beats/breath, spanning
        0.94 breaths - so they are phase-complete BY LUCK. Measured on its actual beats, the breath excess
        over a random sample of the same size is ~0. This is why ref20 must NOT be read as the general case.
        """
        sd, worst = shrlib.breath_group_bias(8, 8.47, shrlib.RAISED_COSINE)
        self.assertLess(sd * 7.65, 0.35)          # < 0.35 dB on ref20's 7.65 dB swing
        self.assertLess(worst * 7.65, 0.40)

    def test_a_typical_group_carries_a_real_error_bar(self):
        """6-9 beats at 3-8.5 beats/breath is the plausible reference regime (HR and RR both rise with
        exertion, so the ratio stays in a narrow band). The bias there is NOT negligible: it is comparable
        to ref20's own 1.08 dB jackknife SE, which is what makes it worth an error bar at all."""
        worst = max(shrlib.breath_group_bias(n, r, shrlib.RAISED_COSINE)[0]
                    for n in (6, 8, 9) for r in np.arange(3.0, 8.51, 0.05))
        self.assertGreater(worst * 7.65, 1.0, "a typical short group should carry >1 dB of breath bias")


class BreathSwingConvergenceTests(unittest.TestCase):
    """Ruler 5: the swing is an aliased statistic. These are the tests that would have caught it."""

    # Beats-per-breath ratios that are NOT near a low-denominator rational, so a rigid rotation
    # equidistributes over the breath curve. 6.77 is the ref20 calibration point (HR 176 / RR 26).
    WELL_COVERED = (4.44, 6.77, 7.5, 13.3)

    def test_swing_converges_to_the_true_value_when_the_phase_is_covered(self):
        for beats_per_breath in self.WELL_COVERED:
            n_beats = int(round(ro.MIN_BREATH_CYCLES * beats_per_breath))
            inflations = beat_sampled_inflations(beats_per_breath, n_beats)
            measured = ro.breath_swing_db(muffle(inflations), inflations)
            self.assertAlmostEqual(measured, true_swing_db(), delta=0.10)

    def test_swing_is_stable_under_doubling_the_sample_count(self):
        for beats_per_breath in self.WELL_COVERED:
            n_beats = int(round(ro.MIN_BREATH_CYCLES * beats_per_breath))
            single = beat_sampled_inflations(beats_per_breath, n_beats)
            double = beat_sampled_inflations(beats_per_breath, 2 * n_beats)
            self.assertLess(
                abs(ro.breath_swing_db(muffle(single), single)
                    - ro.breath_swing_db(muffle(double), double)),
                0.10)

    def test_swing_is_NOT_stable_below_the_cycle_guard(self):
        """Characterization test: the guard is load-bearing, so it cannot be silently removed.

        A 24-beat run carries several tenths of a dB of pure sampling noise over plausible
        beats-per-breath ratios, comparable to the calibration acceptance band.
        """
        errors = []
        for beats_per_breath in self.WELL_COVERED:
            inflations = beat_sampled_inflations(beats_per_breath, 24)
            errors.append(abs(ro.breath_swing_db(muffle(inflations), inflations) - true_swing_db()))
        self.assertGreater(max(errors), 0.30)

    def test_short_runs_are_reported_as_unsound(self):
        inflations = beat_sampled_inflations(6.77, 24)          # ~3.5 cycles: deliberately unsound
        problems = ro.breath_swing_problems(inflations, hr=176.0, resp_rate=26.0, curve=shrlib.SINE)
        self.assertTrue(any("respiratory cycles" in problem for problem in problems))


class BreathPhaseLockTests(unittest.TestCase):
    """The failure MIN_BREATH_CYCLES cannot see, and that 'sweep until it stops moving' certifies.

    The beats advance the respiratory phase by a fixed increment, so at a low-denominator beats-per-breath
    ratio they revisit the same handful of phases forever. At exertion 1 the RSA term is exactly zero, so
    the advance really is a rigid rotation - and HR 200 against MaxRespRate 50 is exactly 4.0 beats per
    breath, an operating point the engine genuinely reaches.
    """

    LOCKED = 4.0        # HR 200 / RR 50: the maximum-exertion operating point
    UNLOCKED = 4.44     # a neighbouring ratio that equidistributes

    def test_a_locked_ratio_never_converges_no_matter_how_many_beats(self):
        for cycles in (10, 30, 100, 400):
            n_beats = int(round(cycles * self.LOCKED))
            inflations = beat_sampled_inflations(self.LOCKED, n_beats)
            error = abs(ro.breath_swing_db(muffle(inflations), inflations) - true_swing_db())
            self.assertGreater(error, 1.0, f"expected a locked ruler to stay wrong at {cycles} cycles")

    def test_a_locked_ruler_looks_stable_which_is_why_a_sample_sweep_cannot_catch_it(self):
        """The trap, pinned: doubling the sample count moves the locked value hardly at all. Any guard
        phrased as 'sweep the sample count until the value stops moving' would certify this reading."""
        single = beat_sampled_inflations(self.LOCKED, 400)
        double = beat_sampled_inflations(self.LOCKED, 800)
        self.assertLess(
            abs(ro.breath_swing_db(muffle(single), single)
                - ro.breath_swing_db(muffle(double), double)),
            0.01)

    def test_the_coverage_guard_catches_what_the_cycle_guard_misses(self):
        n_beats = int(round(100 * self.LOCKED))                 # far past MIN_BREATH_CYCLES
        inflations = beat_sampled_inflations(self.LOCKED, n_beats)
        problems = ro.breath_swing_problems(inflations, hr=200.0, resp_rate=50.0, curve=shrlib.SINE)
        self.assertTrue(any("under-covered" in problem for problem in problems))
        self.assertFalse(any("respiratory cycles" in problem for problem in problems),
                         "the cycle guard is satisfied here - only the coverage guard can see this")

    def test_the_coverage_guard_passes_a_well_covered_run(self):
        hr, resp_rate = 178.0, 178.0 / self.UNLOCKED
        n_beats = int(np.ceil(ro.MIN_BREATH_CYCLES * self.UNLOCKED)) + 1
        inflations = beat_sampled_inflations(self.UNLOCKED, n_beats)
        self.assertEqual(ro.breath_swing_problems(inflations, hr, resp_rate, curve=shrlib.SINE), [])

    def test_the_coverage_tolerance_bounds_the_swing_error(self):
        """BREATH_MEDIAN_TOL is not a taste value - it is chosen so that what it passes is accurate enough.

        Sweep the plausible beats-per-breath range as a RIGID rotation (the exertion-1 worst case) and pin
        the worst aliasing error the guard lets through. 0.30 dB is the measured bound, not an aspiration:
        it occurs just off the 4.0 lock, where the group medians are still nearly right but the phases are
        not. The RSA-live points that calibration actually runs at sit far inside this, at <=0.011
        deviation. If this test starts failing, the guard has been loosened - do not raise the bound.
        """
        worst = 0.0
        for beats_per_breath in np.arange(3.0, 15.001, 0.005):
            n_beats = int(round(ro.MIN_BREATH_CYCLES * beats_per_breath))
            inflations = beat_sampled_inflations(beats_per_breath, n_beats)
            if ro.breath_swing_problems(inflations, hr=180.0, resp_rate=180.0 / beats_per_breath, curve=shrlib.SINE):
                continue
            worst = max(worst, abs(ro.breath_swing_db(muffle(inflations), inflations) - true_swing_db()))
        self.assertLess(worst, 0.30, f"guard now admits {worst:.3f} dB of aliasing error")


@unittest.skipUnless(
    SOURCE.is_file() and core_offline.binding_available(),
    f"source sample or compiled binding unavailable: {SOURCE}",
)
class WindowRuleTests(unittest.TestCase):
    """One estimator, one declared window rule.

    These render real beats through the compiled core, so they are the slow tests here - a short run is
    enough, since they assert which window a metric is measured in, not what the value converges to.
    """

    @classmethod
    def setUpClass(cls):
        cls.seq = ro.beat_sequence(SOURCE, hr=176.0, contractility=0.75, exertion=0.75,
                                   n_beats=8, vigor_sigma=0.0, seed=0)

    def test_metadata_covers_every_measure_sequence_output(self):
        measured = ro.measure_sequence(self.seq, dur_ms=128.0, warmup=1)
        self.assertEqual(set(measured), set(ro.ESTIMATOR_METADATA))
        for metadata in ro.ESTIMATOR_METADATA.values():
            self.assertIn(metadata["window"], {"onset+dur_ms", "beat+fixed", "sequence"})
            self.assertIn(metadata["width"], {"invariant", "sensitive"})
            self.assertIn(metadata["converges"], {None, "breath-cycles"})

    def test_compiled_sequence_preserves_native_layout(self):
        self.assertEqual(self.seq["audio"].ndim, 2)
        self.assertEqual(self.seq["audio"].shape[1], 2)

    def test_the_breath_window_is_independent_of_the_ref8_dur_ms_knob(self):
        """Nothing measured in the fixed breath window may depend on the onset-window dur_ms; assert
        bit-identical output, not merely a close value."""
        narrow = ro.measure_sequence(self.seq, dur_ms=75.0, warmup=1)
        wide = ro.measure_sequence(self.seq, dur_ms=160.0, warmup=1)
        for key, metadata in ro.ESTIMATOR_METADATA.items():
            if metadata["window"] == "beat+fixed":
                self.assertEqual(narrow[key], wide[key],
                                 f"{key} is declared beat+fixed but moved with dur_ms")

    def test_brightness_is_measured_in_the_onset_window_not_the_breath_window(self):
        """Assert anchoring directly: per-beat centroid must exactly reproduce _s1_window and not the
        breath window. An indirect width-sensitivity check cannot distinguish the two anchors.
        """
        dur_ms = 128.0
        measured = ro.measure_sequence(self.seq, dur_ms=dur_ms, warmup=0)
        for centroid, beat in zip(measured["centroid"], self.seq["beats"]):
            onset_window = ro._s1_window(
                beat["audio"],
                beat["systole_duration"],
                dur_ms,
            )
            breath_window = ro._fixed_s1_window(beat["audio"])
            self.assertAlmostEqual(centroid, shrlib.centroid(onset_window, SR), places=9)
            self.assertNotAlmostEqual(centroid, shrlib.centroid(breath_window, SR), places=3)

    def test_declared_width_invariance_holds(self):
        """The width axis of ESTIMATOR_METADATA, pinned in both directions. 'invariant' must be EXACT (a
        peak is a max, and the S1 peak sits inside even the narrowest window, so it saturates - which is
        what makes the peak-amplitude CV calibration robust to a window change). 'sensitive' must actually
        move, or the declaration is hiding that the metric is anchored somewhere else entirely.
        """
        narrow = ro.measure_sequence(self.seq, dur_ms=75.0, warmup=1)
        wide = ro.measure_sequence(self.seq, dur_ms=160.0, warmup=1)
        for key, metadata in ro.ESTIMATOR_METADATA.items():
            if metadata["window"] != "onset+dur_ms":
                continue
            same = np.allclose(np.asarray(narrow[key], float), np.asarray(wide[key], float))
            if metadata["width"] == "invariant":
                self.assertTrue(same, f"{key} is declared width-invariant but moved with dur_ms")
            else:
                self.assertFalse(same, f"{key} is declared width-sensitive but ignored dur_ms")

    def test_the_s1_window_starts_at_the_onset_and_contains_the_whole_rise(self):
        """The last-crossing rule can reset at an envelope null and discard the rise. A window that starts
        at a true onset opens near silence and peaks later; one that starts at the peak does not.
        """
        for beat in self.seq["beats"]:
            window = ro._s1_window(
                beat["audio"],
                beat["systole_duration"],
                dur_ms=128.0,
            )
            envelope = shrlib.env_analytic(window, SR)
            peak = float(envelope.max())
            self.assertLess(envelope[0], 0.15 * peak,
                            "window opens at a loud sample - it started at/after the S1 peak")
            self.assertGreater(int(np.argmax(envelope)) / SR * 1e3, 5.0,
                               "window peaks immediately - the rise is outside it")


if __name__ == "__main__":
    unittest.main()
