# Measurement Methods

This document owns the cross-cutting rules for measuring reference recordings, offline renders, and
in-game captures. [REFERENCE_ANALYSIS.md](REFERENCE_ANALYSIS.md) owns the findings produced by these
methods; code and tool docstrings own each local algorithm and its executable preconditions.

The central rule is simple: a number is evidence only after the ruler has been shown to measure the
intended feature under the comparison being made. A stable or plausible-looking result is not enough.

## Validity before value

Before trusting a measurement:

1. Name the **estimand**: the physical or perceptual feature the number is intended to represent.
2. Name the **comparison class**: within one beat, across beats, across a state ramp, across recordings,
   or engine versus reference.
3. Identify every anchor, window, filter, and aggregation rule. An `argmax`, a relative threshold, a
   hand annotation, and a closed-form coverage target are all anchors.
4. Test the ruler on synthetic signals with known ground truth. Assert invariances to every nuisance
   variable that should not change the answer, including level, fundamental frequency, lobe structure,
   window width, start phase, and sample count where applicable.
5. Inspect the waveform or spectrogram with the ruler's anchors drawn on it whenever a result is
   surprising. A decision still must be made about what an annotation means; another inferred metric
   cannot arbitrate an annotation convention.

Anchor-free metrics are preferred for cross-signal comparisons. An anchored metric may still be useful
inside a declared validity domain, but that domain must be demonstrated rather than assumed.

The synthetic fixture in `tests/test_metrics.py` supports a deterministic white-noise floor. Its 20 dB-SNR
contract demonstrates that `rise_10_90_ms` and the explicitly bounded F0 ruler remain within their
declared tolerances across 40-200 Hz on a single effective onset; it does not grant noise invariance to
centroid, lobe count, or other rulers whose estimand genuinely changes when broadband floor energy is
added. The rise ruler has an invalid multi-component domain: moving an earlier component from just below
to just above the relative 10% threshold changes which component starts the clock and moves the result
discontinuously while the main rise remains identical. The multi-component fixture fixes this boundary.

## Recording-chain confounds

Reference recordings contain microphone/stethoscope response, placement, automatic gain control (AGC),
limiting, compression, codec effects, and noise in addition to physiology. "Within-recording" is not a
validity guarantee: a comparison survives only confounds that cannot act over the span being measured.

- **Within one beat:** timing and ratios such as S2/S1 level are generally safe from AGC because the
  gain cannot change materially inside the roughly 300 ms comparison.
- **Across consecutive beats:** timing remains safe. Level spread may already be compressed by AGC and
  must be treated as a guardrail unless an AGC-invariant ruler is available.
- **Across a drive or recovery ramp:** absolute level is unsafe even within one file. The ramp is exactly
  the timescale on which AGC acts. Use physiology or a perceptual comparison for the level law.
- **Across recordings:** absolute level and spectral centroid do not transfer because the sensor,
  placement, gain, and codec differ. Within-beat ratios and timing can transfer when their own anchors
  are valid.
- **In-game lossy captures:** use them for within-capture ratios, peak/headroom checks, and gross
  regressions. Use a lossless capture for a small absolute spectral claim.

`ref8` is a timbral target, not physiological evidence. A match to it may be a deliberate sound-design
choice, but it must not establish a general claim about hearts.

## Preferred rulers

| Comparison | Preferred ruler | Required conditions and interpretation |
|---|---|---|
| S1/S2 onset-to-onset timing | Hand `s1a`/`s2a` landmarks; group median | Mark the earliest credible acoustic onset of each whole sound complex. For S2 this is normally A2-like; a later P2/clap never replaces `s2a`. Keep state transitions out of steady-state fits. `auto_annotate.py` gates each onset directly by median absolute error and separately gates systole drift. |
| Rise time across signals | `shrlib.rise_10_90_ms` | Analytic envelope; running maximum makes it immune to pre-peak nulls. Start from an S1 annotation so pre-S1 energy cannot start the clock. Every compared S1 must also keep any earlier component safely on the same side of the relative 10% threshold; a precursor near that threshold can switch the starting component discontinuously. |
| Local build used by the S1 compressor | `shrlib.onset_peak_idx` / `FindBaselineAttackRegion` in `HeartbeatSource.cpp` | Last 10%-of-peak crossing on the analytic envelope, cached before beat effects. Valid for this source's final ascent, not for comparing unlike lobe structures. C++ and NumPy definitions must remain identical. |
| HF timing across unlike S1 lobes | `shrlib.hf_temporal_skew` | HF-energy temporal centroid minus broadband-energy temporal centroid, normalized by window duration. Negative means HF leads; positive means it trails. The window must be the complete actual S1, and comparisons must match the window/S1-duration fraction. Material clipping or saturation invalidates it by manufacturing time-localized HF. Use the group median as a same-path lead/lag diagnostic and late-HF-wash detector, not as a perceptual sharpness ordering, drive proxy, cross-recording rank, or setpoint. |
| Rise/body HF balance in like lobes | `shrlib.rise_body_contrast` plus `shrlib.lobe_count` | Fixed peak-anchored windows, whole-file zero-phase bandpass. Direction indicator only. Compare ref8 with the single-lobe engine or one signal across a sweep; do not compare multi-lobe references. |
| S2 valve clap | A-weighted, peak-anchored HF share from `tools/measure_clap.py` | Fixed short window around the S2 envelope peak; tail-immune. Broadband S2 centroid is dominated by the fundamental and can hide the clap. |
| General spectral balance | Magnitude centroid and fixed band fractions | Match window, anchor, weighting, level, and capture path. Prefer the 80-200 Hz band for this source when the low fundamental makes centroid move opposite perceived brightness. Absolute cross-recording values are descriptive only. |
| Fundamental frequency | `shrlib.f0` only on a clean, unimodal spectral peak | Validate the estimator on synthetic signals spanning the relevant F0, muffle, noise, and window conditions. A muffled low-F0 window needs a validated autocorrelation/cepstral alternative before it can drive a coefficient. |
| Breath loudness swing | `shrlib.breath_swing_db` | A-weight the whole signal, then use fixed 75 ms S1 windows and top/bottom 30% inflation groups. Engine calibration runs need both the minimum-cycle and phase-coverage guards. Short references must report cycle-level uncertainty and act as guardrails rather than sub-dB setpoints. |
| Beat-to-beat peak spread | Peak-amplitude CV on matched onset windows | Peak CV is width-invariant once the peak is included, but cross-recording spread remains state- and AGC-confounded. Crest and centroid CV are width-sensitive. |
| HF energy over time | Time-domain `sum(y^2)` after one whole-signal bandpass | Use equal-duration bins. Do not compare sums of zero-padded FFT magnitude across unequal windows; they are not additive. |

`tools/shrlib.py: METRIC_METADATA` declares primitive anchors and cross-signal validity.
`tools/rhythm_offline.py: ESTIMATOR_METADATA` declares each composite estimator's window,
width-sensitivity, and convergence requirement. Tests must fail when a new output omits that metadata.

The automatic S2 detector follows the same convention without pretending it can identify A2/P2 anatomy:
its LF floor walk is primary, and a qualified 150-400 Hz onset only replaces the inter-lobe valley when
the LF walk never crossed its floor and the HF attack is demonstrably a renewed, local event. This is an
onset fallback, not a clap-present classifier. Normal A2 precedes P2 and expert PCG datasets segment S2 as
one fundamental sound state; see the [CirCor dataset method](https://physionet.org/content/circor-heart-sound/1.0.0/)
and [Curtiss et al. on normal A2-P2 splitting](https://doi.org/10.1161/01.CIR.51.1.157).

## Diagnostic anchor views

`tools/plot_metric_anchors.py` renders the standard review view for a surprising temporal measurement.
Its upper panel is a contextual spectrogram; its lower panel shows the raw waveform, the exact analytic
and lobe-detector envelopes, running maximum, annotation boundaries, threshold crossings, envelope peak,
detected lobe peaks, and peak-anchored rise/body windows. Both panels also show the broadband and HF
energy temporal centroids used by `hf_temporal_skew`. They share one x-axis geometry so every temporal
landmark aligns vertically.

The fixed review defaults are a 0-1000 Hz spectrogram, 32 ms Hann window, 4 ms hop, and -60..0 dB range
relative to the view peak. The frequency ceiling and display parameters remain explicit command-line
overrides and are printed on the figure. The spectrogram supplies morphological and noise context only:
its time/frequency resolution tradeoff cannot overrule an onset located on the waveform or analytic
envelope.

Supply the annotation/window numerically so the same tool works for references, engine renders, and
synthetic fixtures:

```text
python tools/plot_metric_anchors.py recording.wav --start 12.345 --end 12.470 --out anchors.png
```

Audacity remains the playback and annotation editor; the static view is the reproducible record of what
the executable rulers saw.

## Windows, envelopes, and filtering

- State every time anchor and window width beside an absolute time or window-sensitive result.
- Compare equal windows unless the estimator has a proven normalization for unequal ones.
- For `hf_temporal_skew`, "equal" means an equal fraction of the complete actual S1 duration, not equal
  milliseconds or a generic fraction of systole. Even trailing zero padding changes the normalized
  magnitude; truncating a later S1 lobe can reverse the sign.
- Filter a complete recording or run once, then slice measurement windows from it. Per-window FFT
  filtering creates circular convolution; per-window causal filtering adds edge transients at the very
  onset being measured.
- For time-local HF work, use a zero-phase whole-signal bandpass and time-domain energy.
- Use the analytic (Hilbert) envelope for onset and decay. A lightly smoothed rectified waveform retains
  half-cycle zeroes and turns rise time into approximately a quarter-period measurement.
- Window-width stability is an assertion about a declared domain, not a property inferred from one
  successful sweep. A fixed 128 ms S1 window can contain very different fractions of systole at rest and
  high HR.

## Periodic sampling and aggregation

Beat samples advance through respiratory phase by a structured rotation; they are not independent random
phase samples. Two independent checks are required for a breath statistic:

- enough complete respiratory cycles (`shrlib.MIN_BREATH_CYCLES`); and
- representative phase coverage (`shrlib.breath_swing_problems`).

Increasing the beat count is not a coverage fix. At a low-denominator beats-per-breath ratio, the same few
phases repeat forever and the wrong answer can appear perfectly converged. Coverage targets must name the
inflation curve that derived them: the engine's sine curve and the references' raised-cosine reconstruction
have different distributions.

Reference groups are semantic descriptions of posture, site, activity, and recording quality, not
statistical sampling units. A median over a short group can carry respiratory-phase bias, ordinary
beat-to-beat error, and state drift. Report group size and spread, preserve unknown state, and split a
transition from a steady-state group rather than letting the median mix regimes.

## Calibration and provenance

A calibration target must be reproducible from committed evidence:

- A distinctive prose value cites a leaf in `references/measurements.json` through
  `tools/data_citations.toml` where the checker supports it.
- A reference-calibrated constant names the leaf, ruler, reference state, engine result on that same
  ruler, and tolerance in `[[calibrate]]`.
- A fit is recomputed from leaves. Prose-only input points are not a reproducible evidence base.
- A constant without a compatible leaf is `[[uncited]]` with the reason; a suspect measurement is not
  made trustworthy by binding it.
- An ear-set value says so. If the reference statistic's confidence interval is wider than the audible
  discrimination, the ear is the anchor and the statistic is only a drift guardrail.

### Reference-state ledger

Reference state is authored per measurement scope in `references/state_ledger.toml`; `ref_analyze --all`
validates and embeds it beside the generated values in `references/measurements.json`. Every group carries
heart rate, effort phase, demand, respiratory rate and depth, contractility, fatigue, posture,
auscultation site, and rhythm. A mandatory dimension remains present when unknown. Maneuver-specific
dimensions are optional; `lung_inflation`, for example, records an end-inspiratory hold without pretending
that respiratory phase measures tidal excursion. `tools/reference_states.py` defines and describes the
mandatory fields, optional fields, statuses, unknown reasons, and observation fields.

Known values distinguish direct measurement, reported context, estimates, and contextual inference.
Unknown values say why: `not_yet_sourced` is unfinished research, `not_recorded` is absent from the
available reference, `not_identifiable` is confounded or circular in the available evidence, and
`not_applicable` is outside the scope. The first is open ledger debt; the others can be final results.

Contractility may be classified qualitatively from independent protocol context such as rest, active
exercise, immediate recovery, or acute arousal. Do not infer its coordinate from S1 level, attack,
brightness, or systole when calibrating those same consequences. The current recordings provide no
independent numerical contractility coordinate: acoustic S1 attack is not a dP/dt measurement, and audible
S1-to-S2 combines PEP with LVET, preload, and afterload effects.

Each `[[calibrate]]` entry names a generated `state_key` and explains compatibility. Engine fixture choices
remain in `state_compatibility`; they are not copied into observed state. A mixed-state fit names every
contributing group state. `tools/check_data_citations.py` verifies those paths and records state-limited
uncited constants through `state_keys`.

Composite estimators need synthetic tests just as primitives do. A correct primitive can still be placed
in the wrong window, aggregated over a biased phase sample, or compared with a target derived for another
model.

## Engine/reference parity

`tools/engine_offline.py` mirrors the C++ sinus DSP stages but does not model the full `Runtime`;
per-beat filling, jitter, rhythm scheduling, and acoustic mapping require the appropriate sequence
harness. Its normalized-stereo float path is checked against C++ at the conditioned source, per-beat
source, transmission, transducer-input, and final-output domains for the five core fixtures. The
legacy mono audition renderer remains a duplicate reference rather than a golden implementation. Its
current `is_pvc` option covers the voice's shaping bypasses and resample, not
`CreateRenderSpec`-assigned PVC amplitudes or systole, and must not be used as a PVC calibration fixture
until WI-010 closes that gap.
Any C++ DSP change must be mirrored and checked at the same operating state. A comparison must align:

- physiological state and per-beat variability;
- signal-chain stages and constant set;
- time anchor and window width;
- filtering and weighting; and
- capture format and loudness.

When a result cannot meet those conditions, label it directional, provisional, or unknown instead of
turning the mismatch into a tuning target.
