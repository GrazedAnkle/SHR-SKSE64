# Reference Recording Analysis

Per-reference findings from our analysis (companion to
[references/timestamps.txt](references/timestamps.txt), which holds annotated timestamps).
Numbers are +/-10ms where from manual annotation. Read
[MEASUREMENT_METHODS.md](MEASUREMENT_METHODS.md) before treating any figure as transferable evidence.

## Measurement validity

Cross-cutting confounds, ruler validity domains, filtering/window conventions, and preferred metrics live
in [MEASUREMENT_METHODS.md](MEASUREMENT_METHODS.md). In particular, "within-recording" is not sufficient:
only comparisons whose timescale is shorter than the recording chain's response, or whose construction
cancels it, survive.

The trusted cross-signal rise ruler is the analytic-envelope `shrlib.rise_10_90_ms`; ref8's peak-group S1
rise is **33.9 ms**. Absolute level and centroid do not transfer across recordings.

Some per-reference absolute centroids and breath-swing percentages come from windows without landmark
files (refs 4, 6, 7, 10, 12a, 13) and are indicative only: the within-recording ratios still hold, but
the absolute figures are not regenerated into [references/measurements.json](references/measurements.json).
Re-annotating those windows is a later evidence-maintenance item in
[ROADMAP.md](ROADMAP.md#evidence-and-engineering-maintenance).

## Systole law

Calibrated from these references: **QS2 about 465 - 1.71*HR (ms)** (S1-to-S2 = electromechanical
systole). See [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md).

The law is anchored to our own reference recordings, and the fit is a **committed leaf**
(`systole_law` in [references/measurements.json](references/measurements.json), from
`ref_analyze.fit_systole_law`): **458.1 - 1.660*HR ms**, residual std **12.3 ms** over **13 groups**,
HR 68-181. The engine's parameter line is **465 - 1.71*HR ms**; its *audible* line includes the +1.9 ms
S1/S2 source lead-in difference and agrees with the reference line at the measured mean HR within the
live prediction SE. The fitted intercept sits ~80 ms below
Weissler's population QS2 (~546): most of that is definitional - we mark the *S1 sound onset*, not the ECG
Q-wave, so the pre-S1 electromechanical interval his QS2 includes is excluded - with the remainder
consistent with our references skewing young/fit and often recorded at elevated sympathetic drive, which
shortens systole below a mixed resting population. The fitted slope (~1.66 ms/bpm) is close to his LVET
(1.7) and below QS2 (2.1), but a 13-point mixed-state fit carries no "LVET not QS2" claim. We model
the interval as the full audible S1-to-S2 (PEP + LVET).

The fit is recomputed only from committed measurement leaves; prose-only points are not part of its
evidence base. Current `s2a` landmarks follow the
[operational onset convention](MEASUREMENT_METHODS.md#preferred-rulers), and hand review confirms that
they mark the S2 rise rather than its body. The detector gate accepts all 11 held-out groups: ref14's
high-release group has +0.6 ms systole drift, and ref11's breath-hold group has 7.9 ms S2
onset p95 error. Its sole named residual is independent of S2: one automatic S1 lies 61.8 ms before its
hand mark, just outside the unchanged 60 ms matcher, while all matched-beat rulers pass. The slope is
robust to a uniform onset bias, while the intercept is not. Both coefficients are bound in
`tools/data_citations.toml`.

---

## Per-reference

### ref1 - resting, regular respiration, supine
- Purpose: low HR steady-state
- Context: clinical recording
- Recording device: stethoscope
- Measured: HR ~88 (auto-detected by tools/auto_annotate.py, robust across windows). Systole ~330ms
  on the clean interior window. Short recording (~10.8s); the noisy first few seconds degrade a
  whole-file pass, but the interior detects cleanly.
- Notes: TODO: Need to re-verify HR if promoted to a landmark file.

### ref2 - resting (elevated), regular respiration, supine
- Purpose: elevated HR steady-state
- Context: clinical recording
- Recording device: stethoscope
- Measured: HR ~93
- Notes:

### ref3 - high HR / post-exercise, regular respiration, seated
- Purpose: high HR steady-state
- Context: home recording
- Recording device: gooseneck mic
- Measured: HR ~145, systole ~206ms (model 208, residual -1). Post-exercise steady-state
  confirmation of the QS2 systole law.
- Notes: some noise (white/electrical) with wide DC offset swings. Cleaned up easily for analysis
  using a high-pass at ~20 Hz and a low-pass at ~3000 Hz. Difficult to discern effect of ventilation
  on S1, S2 auditory character.

### ref4 - resting (elevated), regular respiration, seated
- Purpose: clean recording of elevated HR steady-state
- Context: home recording, nervous
- Recording device: condenser mic, no direct contact
- Measured: HR ~117, systole ~250ms. S1 dynamic range ~10.5 dB.
- Notes: nervous -> sympathetic drive raising HR. Systole sat on the steady-state line (drive raises
  HR and shortens systole together). Confirms drive shows as decoupling only in recovery, not
  steady state.

### ref5 - elevated / post-exercise, regular respiration
- Purpose: elevated HR steady-state
- Context: clinical recording
- Recording device: stethoscope
- Measured: HR ~126, systole ~199ms (-42 below the line).
- Notes: clean recording, but low quality. Short systole suggests drive/recovery decoupling, but
  uncertain without further context.

### ref6 - exam, athlete
- Purpose: HR as posture is varied, HR post-exercise, RSA magnitude, A2-P2 split, possible S1 split
- Context: clinical recording, athlete, more involved exam than previous refs
- Recording device: stethoscope
- Measured: supine resting HR ~48-65 (S1-isolating). Large beat-to-beat HR swing. **Breath-modulation
  REST anchor** (supine, ~62bpm): A-weighted breath-loudness range ~2.3dB. Centroid swing 57-76Hz
  (34%). HF-fraction 0.05 to 0.15 over the breath cycle.
- Notes: S1 is quieter than S2. **Clear A2-P2 split** (a good reference example for the deferred,
  low-priority S2-split work).

### ref7 - high HR / exercise / post-exercise, breath hold, PVCs
- Purpose: high HR, recovery, PVC timing and auditory character
- Context: home recording, bodyweight squats, breath hold with posture change to induce PVCs
- Recording device: stethoscope
- Measured: recovery HR 150 to 109 over ~4.6s. Systole flat ~193ms (std 10), the strongest
  contractility-decoupling evidence. 3 PVCs: coupling ~0.62, full compensatory pause (2x IBI),
  PVC systole ~180-188ms, PVC S2 present.
- Notes: breath-hold suppressed venous return -> LVET stayed short -> fully flat systole (vs the partial
  lengthening in ref13's normal-breathing recovery clip, asset ref12). Benign post-exercise ectopy.

### ref8 - high HR / post-exercise / excited state, regular respiration, seated
- Purpose: high HR steady-state, recovery
- Context: home recording
- Recording device: stethoscope
- Measured: peak HR ~173 (systole ~165, on/near line). Recovery HR ~142 (systole ~202, ~12ms
  below line -> mild hysteresis). **Breath muffle reference:** centroid swing ~45% over 15-30s;
  F0 swing ~16% (loosely coupled to centroid, corr 0.19). S1 **10-90% rise 33.9 ms**, decay 29.2 ms
  (peak group, analytic envelope); peak-group S1 centroid **85.2 Hz** (descriptive timbre, not a
  cross-recording physiological target).
- Notes: vagal HR drop outpacing contractility decay. NOT one continuous take - snippets per the
  timestamps. Strong muffle/centroid reference. ref8's "overdrive" character is rise/body HF
  **contrast** (median 2.63), not attack speed.

### ref9 - resting, regular respiration, elevated supine
- Purpose: PVC morphology, RSA magnitude
- Context: clinical recording
- Recording device: stethoscope
- Measured: sinus HR ~88-107 with RSA +/-~10% (IBI 558-681ms). Sinus systole ~264ms. PVCs at
  coupling ~0.62. Frequent PVCs, near-bigeminy spacing.
- Notes: prominent RSA, frequent benign PVCs (7+), audible S2 during PVCs.

### ref10 - exercise bike, very high HR
- Purpose: extreme HR steady-state, recovery
- Context: clinical recording, stress test
- Recording device: stethoscope
- Measured: HR ~183 (also ~195 in some segments). Systole ~148ms (on line / slightly below QS2).
  High-HR S1 spectral centroid ~94-132 Hz (vs source ~51 Hz). Recovery on the bike, ~190bpm.
  A-weighted breath-loudness range ~3.3dB. Centroid swing 98-137Hz (40%). HF-fraction 0.23 to 0.47
  over the breath cycle.
- Notes: Current highest-HR spectral reference, but has significant, persistent contact noise
  and background speech

### ref11 - multiple positions, tricuspid/pulmonary boundary, deep-inspiration + post-exercise sections
- Purpose: HR as posture is varied, HR post-exercise, deep inspiration windows, RSA
- Context: clinical recording
- Recording device: stethoscope
- Measured: cleanest window (04:51-04:55) rested HR ~65-67. Systole ~330-349ms (low-HR baseline).
  **Within-recording brightness and breath observations - directional only.** These are single windows
  across large temporal gaps (04:51 -> 10:39 is ~6 min); several figures are demoted pending the
  [cross-gap claim audit](work_items/WI-005-cross-gap-audit.md).
  - *Contractility brightening (direction):* S1 centroid rises from rest (04:51) to post-exercise
    (10:39, same supine site), ~72 -> 90Hz. Two single windows ~6 min apart, so this anchors the
    *direction* of drive-brightening, not a calibrated ratio.
  - *Deep-inspiration muffle (direction, physiology-backed):* at peak inspiration (05:19) the S1 centroid
    drops sharply versus expiration - lung inflation attenuates HF transmission. The magnitude (~-34%)
    is indicative (non-landmark window). A concurrent **F0 drop (~-14%) is suspect** - physiologically
    unsupported and measured by a fragile F0 estimator on a muffled ~41Hz signal; do not model a
    respiratory pitch drop on it (see the audit). At the inspiration trough the muffle can outweigh the
    brightening, so a peak-exertion deep-inspiration beat can read slightly duller than rest.
  - *Breath depth:* shallow post-ex breaths (11:43) show ~0% centroid swing, so muffle depth scales with
    tidal volume.
- Notes: the deep-breath markers (04:56-05:27) are the clean breath-phase reference. The absolute
  centroids here are method-inconsistent (04:51 reads 72 vs 112Hz depending on the window) - see the audit.

### ref13 - exercise plus recovery (asset ref12 clip)

#### Recovery clip (asset ref12) - high HR / post-exercise, regular respiration, supine
- Purpose: recovery, clear hysteresis
- Context: clinical recording
- Recording device: stethoscope. Asset ref12 begins at ref13 04:02.912 (within about 1 ms), so generated
  measurements merge it into the ref13 logical reference while retaining the clip-local annotations.
- Auscultation site: initially mitral, slightly offset toward tricuspid. The 00:13.6 reposition moves it
  more exactly onto mitral; the annotated heartbeat group ends immediately before that reposition.
- Measured: recovery HR 158 -> 142 -> 86. Systole on the line at 158/142 (+10/+1) then -58ms at
  86.
- Notes: Shows LVET recovers with HR/SV under *normal* breathing while PEP/contractility lingers.
  Peak expiration beats saturated/distorted - breath swing depth, HF measurements not valid.
- **Breath (first segment, 4 cycles, annotated off the video visual axis):** `InspirationFraction`
  **0.503** (per-cycle 0.51 / 0.47 / 0.53, +/-0.03), RR 19.8/min - the **calibration-grade second
  reference** for that constant (ref20's in-exercise 0.49 was coarse at +/-0.15). Recovery 0.50 and
  exercise 0.49 agree: ventilatory drive pushes I:E toward ~1:1, up from the resting ~1:2 (~0.33). Only
  the landmark timing survives here - the swing/depth are void. See the
  `ref13.breath_scopes."recovery start (~150)"` leaf.

#### Exercise - rising HR (bodyweight squats), regular respiration
- Purpose: HR rise trajectory (the "in-between" between rest and peak)
- Context: clinical recording, activity from 00:25
- Recording device: stethoscope; accompanying video suggests a mitral site, though the exercise footage
  does not pin it with certainty.
- Measured: **rise trajectory** from 5 manually-annotated clean beats (within-recording, so loudness
  comparable). HR 82 -> 180 bpm. As HR climbs: loudness +0 -> +20dB (corr +0.70), centroid 93 ->
  146Hz (corr +0.61, ~1.5x), F0 46 -> 59Hz (corr +0.43, noisy). Loudness and brightness rise with
  exertion; this recording does not resolve whether S1 attack sharpens (its trajectory beats' landmark
  annotations were not kept, so attack is not re-measurable here). That effect is real and established
  on within-recording drive pairs - see the cross-index.
- Notes: confirms the contractility ramp model. The 82bpm beat is not true rest (subject already
  active). Recovery (movement-free hysteresis) is the ref13 recovery clip stored as asset ref12.

### ref14 - excited state, voluntary breath hold, Erb's/pulmonary site
- Purpose (two):
  1. **P2 valve-clap character** and how it changes with contractility, at a bright (pulmonary) site.
  2. **HR/contractility dynamics of a voluntary breath hold** - reference for a possible future
     Valsalva-like feature (see [later rhythm/simulation work](ROADMAP.md#rhythm-and-simulation)).
- Context: home recording, stethoscope, standing, increased anxiety/excitation, then a breath hold
- Recording device: stethoscope (heavily saturated - clipping makes onset/offset annotation hard and
  energy metrics unreliable; treat numbers as indicative, trust the spectrogram/by-ear band)
- Measured - **clap** (within-recording, same Erb's/pulmonary site = a clean contractility/HR contrast):
  baseline ~135 bpm vs post-breath-hold release ~158-160 bpm. The P2 "clap" is a brief HF transient
  (center bandpass-isolated at ~260 Hz; spectrogram shows energy to ~550 Hz at +25-40 ms) and
  **grows with contractility** - the 220-320 Hz band rises x1.77 baseline->release, 320-500 Hz x2.0;
  A-weighted HF share of S2 rises 58%->79%.
- Measured - **breath-hold dynamics** (per `references/timestamps.txt`): HR rises during the hold
  (02:22-02:47) -> stays high at release (02:48-02:50) -> **drops** (02:51-02:56) -> rises again
  (02:57-end). The post-release dip is a **Valsalva-maneuver response** (phase IV baroreflex
  bradycardia: restored venous return after the strain overshoots blood pressure, and the baroreflex
  brakes HR before it recovers).
- Notes: the broadband S2 *centroid* stays ~53 Hz and the S2/S1 centroid ratio ~0.81-0.88 (the loud
  ~50 Hz fundamental dominates), which is why centroid hides the snap - the ear hears it because it is
  ~30 dB more sensitive at 300-500 Hz than 50 Hz. The clap center is higher here (pulmonary, ~260 Hz)
  than at tricuspid (~120-200 Hz) = site-dependent. Do not use ref14's S1 `hf_temporal_skew` as a
  drive comparison: the post-hold waveform is more severely flat-topped than the already saturated
  baseline, and the changing broadband distortion column manufactures HF inside the S1 dome. This is
  the metric's nonlinear-capture invalid domain, not a physiological HF-timing result.

### ref15 - rest -> activity -> peak, regular respiration
- Purpose: within-recording tonal change rest vs active, HR-rise speed, hysteresis
- Context: stethoscope, activity
- Measured: two clean runs, ~80-102 and ~150-154 bpm (hand-annotated). Within-recording (mic-controlled)
  as HR rises: **S1 brightens** (centroid x1.16, rolloff85 x1.30) with S1 **F0 flat** (x0.94) - the
  rising-pitch-with-HR percept is S1 gaining upper harmonics, not an F0 shift, and it is S1, not S2
  that brightens. S2 centroid flat (x1.00). See annotated table above.
  The A-weighted peak-anchored HF ruler (tail-immune, `tools/measure_clap.py`) confirms this on the
  *perceptual* HF axis, not just the broadband centroid: S1's HF share doubles (14 -> 27%) while S2's
  stays flat (20 -> 22%). ref15 is the clean **no-clap** case (dull mic, no P2 snap), so its S2 has no
  drive response at all - which isolates the P2 clap as the *only* S2-with-drive effect (present at
  clap-bearing sites, ref14/ref11; see the cross-index).
- Notes: a primary within-recording reference for the frequency-vs-HR question (mic held constant).
  Two extra-sound observations (with measurement support - the recording carries a 4-attack pattern:
  doubled-S1, S2, S3 - sounds like an 8th note then two 16ths, with the 8th itself doubled):
  (1) an **S1 double-attack** - a consistent second S1 lobe **~25 ms** after the first (mean 0.18-0.36
  of S1, low std across the 06-34 s runs; present 7/8 ~150 bpm beats). **Not** mere fundamental ripple:
  25 ms exceeds the ~17-20 ms fundamental period and the spacing is stable across HR (ripple would
  track F0). Plausibly M1-T1 or S1's two-component structure; mechanism uninvestigated. (2) A **genuine
  S3**, *additional to* the double-attack. It is **soft**: S1-aligned absolute-ms averaging shows only a
  faint bump at **~S2+125 ms** (the correct early-diastolic S3 window) sitting near the noise floor, so
  measurement can hint but not confirm it - the trained ear is the better instrument for a sound this
  quiet. Heard most clearly in the high-HR segment (01:35-02:01), consistent with a physiologic S3
  growing with output. (The low-HR athlete **ref6** has no audible S3 - a different subject, so not
  evidence against ref15's.) A hand-annotated low-HR ref15 segment (or a pitch-preserving time-stretch)
  would pin it if S3/S4 is picked up (deferred). A "possible split S2" at 01:51.5 is unexamined.

### ref19 - post-exercise near-max HR - `[PROVISIONAL - EAR NOTES ONLY, NOT MEASURED]`
- **Status: not yet analysed. Nothing here may move a constant.** No hand annotation, no
  `references/timestamps/19.txt`, no figures in `measurements.json`. Every number below is a by-ear
  estimate recorded so the observation is not lost; all are explicitly uncertain.
- Purpose (proposed): a candidate near-maximal / post-exercise anchor. The corpus currently has **no
  max-effort reference at all**, which is why the model's extreme end is extrapolated (see the
  extreme-value audit). ref19 is the closest thing available, but see the caveats.
- File: `docs/references/original/19.wav` (90.4 s, 48 kHz, stereo - tools read channel 0).
- Context (by ear): post-exercise, **recovery not peak**. HR ~200 (uncertain). RR roughly 30-40/min, but
  the estimate is weak - not counted against a hand annotation.
- Quality caveats: low recording quality, and **heavily arrhythmic**. Both push against using it as a
  ruler. Beat detection will need validation rather than the `auto_annotate.py` gate alone.
- Observation: by ear ref19 is noticeably **snappier with more HF** than the engine's `max` fixture.
  The comparison is non-actionable: ref19 has a different low-quality recording chain, its recovery depth
  and numerical contractility are not identifiable, and the fixture is maximally muffled. The gap is most
  plausibly capture/source brightness, but the available evidence cannot decompose it. It does not justify
  an envelope or breath-filter retune, and no dedicated state-match render is owed.
- To build out: annotate, measure S1/S2/systole/IBI, confirm the recovery framing, and decide whether the
  arrhythmia load leaves anything usable. Until then this entry is an observation log, not evidence.

---

### ref20 - in-exercise (bodyweight squats), strong breath swing, Erb's/tricuspid
- Purpose: high-HR in-exercise breath modulation - the **breath-muffle calibration anchor** for the
  inspiratory low-pass and amplitude depth (see
  [WI-008](work_items/WI-008-spectral-breath-muffle.md), `BreathLowPassMinHz`, and `BreathAmpDepth`).
- Context: home recording, bodyweight squats, sustained ~178 bpm (in-exercise, not recovery)
- Ventilation context: the five hand-labelled cycles span roughly **24 breaths/min**. The subject and
  protocol skew young/fit and may be below maximum ventilation even at this HR; without the subject's
  measured maximum or gas-exchange thresholds, this is a threshold-region character reference, not an
  exact HR-to-metabolic-demand coordinate.
- Recording device: stethoscope, Erb's/tricuspid (in-exercise contact noise; **S2 is soft and hard to
  discern** - deliberately not used as evidence)
- Measured: 47 hand-verified beats (median-IBI HR ~176, systole ~170 ms / QS2-pred 163, S1 dur 114 ms,
  S2 dur ~52 ms). S2 sits ~13.1 dB below S1 (quiet, approximate). Multi-lobe S1 (2 lobes, runner-up
  0.47x). **Breath muffle (the headline):** the median inspiration beat is duller and quieter than the
  median expiration beat. The swing is a committed leaf (`ref20.breath`, from `BR=` landmarks):
  **-8.11 dB A-weighted**, centroid ratio 0.744, RR 20.6/min, `InspirationFraction` 0.49 (coarse,
  +/-0.15 - the heart samples the breath only ~9x/cycle; ref13's recovery clip (asset ref12) at
  0.50 +/-0.03 is the finer anchor).
  This is the **strong upper end** of breath modulation (mid-exercise), not every breath's target -
  ref6's ~2-3 dB is the low end.
  - **Its uncertainty is +/-1.32 dB and that is the story** (leave-one-breath-out jackknife over its 6
    cycles; `jackknife_se_db`, which the `BreathAmpDepth` guardrail in `tools/data_citations.toml` now
    binds to LIVE, so the tolerance tracks re-annotation instead of freezing stale). So `BreathAmpDepth`
    is **ear-anchored, with this swing as a guardrail**; do not retune it against sub-dB movements here.
    (An older `vent:`-tag cross-check read -7.65 dB, a ~1 dB method spread, but that coarse layer was
    retired when the `BR=` landmarks replaced it.)
  - The reference's raised-cosine coverage deviation is **0.031**, a mild under-coverage rather than the
    engine's phase-lock failure shape. Curve-specific coverage targets are required; see the breath ruler
    in [MEASUREMENT_METHODS.md](MEASUREMENT_METHODS.md#preferred-rulers).
- Notes: **Method robustness.** Phase was hand-assigned from the *loudness* envelope while the headline is a
  *loudness* swing, so selection bias was a live worry. It is measured and bounded at ~0.2 dB:
  phase-from-level (fully circular) gives -8.43 dB, the hand tags -7.65, and phase from the **centroid** (an
  independent axis) -7.83. The annotator was tracking a pattern, not just picking quiet beats. **RSA is NOT
  an available independent axis here** - it fades with exertion, and on ref20 reads 35 br/min against the
  true ~22, correlating with level at only +0.10.
  **Squat impacts are present and audible**: the 2-18 Hz channel carries an impact train at ~50-58/min
  landing within 5-37 ms of 3 of the 5 breath troughs. It is mechanical only, and A-weighting rejects its LF
  thump by design - which is why a visible envelope "lobe" at ~33.5-34.6 s does not appear in the ruler.
  That lobe is **inter-beat noise** (2.7x baseline), not beat amplitude; the beats muffle monotonically
  through it, so there is no second breath there.
  **ref20 CLIPS** - 16 of 44 beats at ~0.95, and the bias is not random (11 of the 14 expiration beats, 0 of
  the 14 inspiration beats). It should cap the loud end and shrink the swing. **It does not**: declipping
  moves the A-weighted swing by 0.00 dB, because ref20's S1 sits at ~46 Hz where A-weighting applies ~-30 dB.
  **Raw-peak and raw-rms figures are NOT protected by that** (the old "raw S1 rms ~0.50" among them); the
  A-weighted leaf is.
  **Source brightness, not our muffle:** ref20's source is intrinsically brighter than ours (S1 centroid
  80 Hz, S2 86 Hz, S2/S1 1.09x), so the engine's residual inspiration-centroid gap vs ref20 is a
  source-brightness / centroid-tilt difference, not a muffle mistune - unreachable without disabling the
  muffle (cf. the metric-anchor "rulers that lie" findings). Respiratory landmarks are `BR=<trough>-<peak>`
  lines in `references/timestamps/20.txt` (the older per-beat `vent:` tags are retained; they turn out to be
  a ~30% grouping, which is what made them comparable to the engine's rule at all). Corrupt/undefined spans
  are `EX=` lines; the recovery tail is deliberately NOT excluded, since recovery is the state we have the
  least reference coverage of.

---

## Annotated S1/S2 measurements (hand-annotated landmarks)

From precise hand-annotated S1/S2 onset+end timestamps (the per-recording landmark files under
[references/timestamps/](references/timestamps/)) - far more reliable than auto-detection (which
fails on quiet/merged S2 and noisy beats). Runs of ~8 consecutive beats per state so ventilation
averages out. Within-recording only.

These rows are **generated**, not hand-maintained: `tools/ref_analyze.py` measures the annotated
beats into `references/measurements.json` (the single source of truth, holding every annotated
group), and the curated within-recording comparisons below are a view of it. Each value is the
median across the group's beats (robust to an annotation outlier - a degenerate S2, a dropped
beat). HR is the median-IBI estimate; centroids are magnitude-weighted (see
[GLOSSARY.md](GLOSSARY.md)). Regenerate the rows with `python tools/ref_analyze.py --all --md-table`
(or `--check` to flag drift between the JSON and a fresh recompute).

| group | HR | systole (QS2 pred) | S1 dur | S2 dur | S2/S1 loud | S1 cen | S2 cen | S2/S1 cen |
|---|---|---|---|---|---|---|---|---|
| ref11 rest | 67 | 348 (350) | 155 | 138 | 0.36 | 72 | 118 | 1.59x |
| ref11 post-ex | 141 | 222 (224) | 113 | 89 | 0.20 | 90 | 128 | 1.37x |
| ref8 high-HR | 181 | 160 (156) | 128 | 83 | 0.10 | 85 | 82 | 0.99x [&dagger;] |
| ref15 ~100 | 102 | 309 (291) | 144 | 95 | 0.66 | 65 | 76 | 1.16x |
| ref15 ~150 | 152 | 214 (205) | 116 | 75 | 0.46 | 80 | 73 | 0.94x |

[&dagger;] ref8's S2 is *not* brighter than its S1 (no bright clap in this dull recording). The broadband
S2 *centroid* ratio is an unreliable "clap" proxy in general: it is dominated by the loud ~50 Hz
fundamental (see the P2-clap finding in [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md) coupling 2). Only
the within-recording ratio transfers across recordings; absolute centroids depend on mic/method and
are not comparable.

---

## Cross-index by phenomenon

- **Resting baseline (systole/HR):** ref11 (67), ref1 (67), ref6 (48), ref2 (93)
- **Steady elevated (on the line):** ref4 (117), ref10 (183), ref8-peak (173), ref13 recovery clip
  (asset ref12, 158)
- **Contractility decoupling (recovery):** ref7 (breath-hold, flat), ref13 recovery clip (asset ref12;
  normal breathing, partial), ref8 (mild)
- **HR-rise trajectory (rest->exertion):** ref13 (loudness/centroid up; attack-vs-drive is not
  resolvable in this recording, see ref13)
- **S1 rise sharpens with contractility** (analytic envelope). Literature
  predicts it (S1 sharpness tracks dP/dt - see the intensity-determinants note in
  [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md#second-order-couplings)), and the *within-recording* drive pairs
  bear it out where ref13 could not.
  Medians over each annotated group; `t_floor` measures onset from one **absolute** floor per recording
  (its diastolic rms), so the level confound is controlled - a louder beat crosses a fixed floor
  *earlier*, which would **lengthen** `t_floor`. It shortens anyway:

  | pair | 10-90% rise | t_floor | peak ratio |
  |---|---|---|---|---|
  | ref11 resting -> post-exercise | -21.9 ms | -28.6 ms | 1.78x |
  | ref14 elevated -> high, post-hold | -7.4 ms | -14.3 ms | 1.22x |
  | ref15 ~80 -> ~150 bpm | **+7.8 ms** | -0.8 ms | 1.70x |

  Two of three pairs sharpen on the level-invariant 10-90% rise time.
  **ref15 dissents (its 10-90% rise lengthens) for a known reason** - see multi-lobe S1 below: its
  envelope peak flips between S1's lobes across beats, so `attack_ms` measures to a different lobe on
  each. Discount ref15 here. Note each pair also differs in HR, so drive and rate are confounded; this
  establishes the *direction*, not a coefficient. The `peak ratio` and `t_floor` columns are
  absolute-level quantities read across a large within-recording gap, so they are AGC-exposed; the
  level-invariant 10-90% rise is the trusted column (see
  [WI-005](work_items/WI-005-cross-gap-audit.md)).
- **Multi-lobe S1 is the norm, and it destabilizes any peak-anchored metric.**
  S1 is physiologically multi-component (M1 mitral then T1 tricuspid closure). Median lobe count per
  group (5 ms analytic envelope, lobes >=45% of peak and >=15 ms apart), with the runner-up lobe's
  height as a fraction of the tallest:

  | | ref8 | ref20 | ref21 | ref11 | ref13 | ref14 | ref15 |
  |---|---|---|---|---|---|---|---|
  | lobes | **1** | 2 | 2 | 2-3 | 2 | 2-3 | 2-3 |
  | runner-up / peak | 0.00 | 0.47 | 0.55 | 0.62-0.90 | 0.46 | 0.77-0.85 | 0.43-0.81 |

  **ref8 is the only reliably single-lobe recording** - plausibly why it reads as the smooth, dull one,
  and why it is the closest perceptual match to our source. Everywhere else `argmax` flips
  lobe between beats: ref15's `~150bpm` group spans rise/body contrast **0.75** (peak on lobe 1) to
  **80.39** (peak on lobe 3). Consequences: `s1_attack_ms` and `s1_contrast` are only comparable between
  signals of the same lobe structure (ref8 vs the engine), and `s1_lobes` / `s1_lobe_runnerup` are now
  emitted alongside them so the instability is visible.

  `s1_hf_skew` avoids the peak anchor but not the window or recording-chain constraints. It is valid
  only over the complete actual S1, at a matched window/S1-duration fraction, and without material
  nonlinear distortion. Synthetic two-lobe signals show that truncation can reverse its sign, while
  synthetic saturation can turn a known HF lead into a lag. On the complete hand-annotated S1s, the
  clean ref11 and ref15 pairs keep HF leading at both states but the lead shrinks with drive - matching
  the visibly less-slanted high-drive spectrogram, not establishing an inverse sharpness law. Ref14 is
  excluded because its saturation changes between states. Use skew as a same-path HF lead/lag diagnostic
  and late-HF-wash detector, never a perceptual ordering, drive proxy, cross-recording rank, or setpoint.
- **Late S1 energy does not establish a synthetic tail.** Fixed S1-onset-relative 20-100 Hz windows show
  ref8's late energy decaying to its adjacent background before S2 rather than literally overlapping it.
  Several other high-rate groups retain material late-S1 energy. This establishes that late structure is
  plausible, but raw phase alignment cannot distinguish a coherent modal ring from energy-only
  valve/tissue/flow sound and does not identify a resonator frequency. Ref8 remains a timbral target, not
  physiological evidence. The corresponding engine design belongs in
  [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md#second-order-couplings).
- **Respiratory sinus arrhythmia:** ref6 (strong), ref9 (+/-10%), ref11
- **PVCs:** ref9 (7+, full morphology), ref7 (3)
- **Breath muffle observations:** ref11 deep-breath (indicative centroid -34%; the concurrent F0 -14%
  is suspect under [WI-008](work_items/WI-008-spectral-breath-muffle.md)), ref8
  (centroid ~45% over 15-30s), ref6 (34% at rest), ref10 (40%), ref12a, **ref20** (in-exercise ~178 bpm,
  centroid ratio ~0.77 / A-weighted ~-8 dB - the strong *upper* end; the breath-muffle calibration
  anchor, see the ref20 entry above)
- **Loud-beat "overdrive" (rise/body HF contrast, not distortion and not attack speed):** ref8
  (contrast median 2.63 over 8 peak beats, spread 1.77-18.16). What distinguishes an "overdrive" beat
  is HF concentrated in the *rise* relative to the body - a contrast phenomenon, not a faster attack
  (ref8's S1 10-90% rise is 33.9 ms, comparable to our post-low-cut source)
- **S2 split opportunity:** ref11 deep inspiration (04:56-05:27), ref6 (clear A2-P2)
- **S2 P2 valve-clap (an A-weighted HF transient, not broadband centroid):** ref14 (pulmonary,
  ~260 Hz, grows x1.77 with contractility), ref11 rest (between tricuspid and pulmonary, ~120-160 Hz). ref8/ref15 (dull
  mics) have little snap.
- **Frequency rises with HR = S1 brightening, not F0, and not S2** (within-recording, mic-controlled):
  ref15 provides the clean magnitude (S1 cen x1.16 with F0 and S2 flat). Ref11 moves in the same
  direction, but its x1.25 centroid magnitude is method-sensitive across the long gap and remains
  directional under [WI-005](work_items/WI-005-cross-gap-audit.md).
  Confirmed on the **A-weighted peak-anchored HF ruler** (`tools/measure_clap.py`; a short window on
  the S2 envelope peak, so it is tail-immune - the soft LF S2 tail cannot pull it, unlike the
  full-window magnitude centroid): S1's A-weighted HF share roughly doubles/triples with drive
  (ref15 14 -> 27%, ref11 16 -> 49%) while S2's **broadband** stays flat. **S2's only drive response is
  the site-dependent P2 clap** (A-weighted HF: ref14 pulmonary 58 -> 79%, ref11 at the
  tricuspid/pulmonary boundary weaker;
  **absent** at ref15's dull no-clap site, S2 HF flat 20 -> 22%) - already deferred (see below). There is
  **no new S2 drive-dependence to model**: broadband brightness
  and fundamental static, S1 the sole general drive-brightness responder, the clap the lone exception.
- **Beat-to-beat micro-variation:** ref8 peak (IBI CV ~3.9%, S1 peak-amplitude CV ~25.6%) and ref15
  show fast loudness/brightness variation plus a separable slow respiratory component. The engine models
  the fast component with contractility-scaled vigor jitter and the slow component with breath modulation;
  see [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md#second-order-couplings), coupling 12. The reference magnitudes
  remain state- and recording-chain-confounded, so they are guardrails rather than direct coefficients.
- **Position changes:** ref6, ref11
- **High-HR spectral:** ref10 (centroid ~94-132 Hz)
- **Clean envelope/timbre:** ref4 (condenser)

**Spectral drive finding - grounds contractility brightening:** ref15's clean within-recording pair shows
S1 centroid rising x1.16 with F0 and S2 flat. Ref11 independently supports the direction, not a calibrated
ratio, until its window baselines are reconciled. These observations ground the contractility-scaled S1
brightening the engine produces through onset compression (see
[SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md#sourcing-and-shaping)).

## Analyzing our own output (compiled core and in-game captures)

Measuring the engine - to compare against the references above - reuses the same metric battery
(`tools/shrlib.py`), plus the compiled offline path and the capture caveats below.

**Compiled offline path.** The compiled core drives offline analysis directly through the Python binding (see
[ARCHITECTURE.md](ARCHITECTURE.md#offline-execution)), so measuring the engine reuses the same
`CreateRenderSpec`, rhythm, and DSP the plugin ships rather than a reimplementation, and its output is
pinned against regression by the golden manifests under `tests/golden/`. `tools/audition_core.py` writes
the compiled renderer's native stereo output for listening; engine measurements explicitly select channel
zero before using the mono rulers in `shrlib`. The steady-state rhythm and trajectory clients also consume
the binding directly. Retired tail/tamer reproduction starts from the compiled post-onset source stage and
returns to the compiled downstream renderer.

**In-game captures** (a recording of the running mod) confirm the shipped DLL matches the compiled
offline render.
Extract the audio with `ffmpeg -map 0:a:0 -ac 2 -ar 48000`. The heartbeat voice is centred (mono), so a
clean capture has zero stereo side energy - any side content means other game audio leaked in and the
capture is not analyzable. Detect beats from the smoothed energy envelope, then **disambiguate S1 from
S2**: drop any peak within ~0.45s of a louder neighbor at <=0.6x its height. Without this the detector
double-counts S2 at low HR (where S2 lands ~350ms after S1, outside the ~0.28s refractory) and reports
~2x HR at rest; at high HR the systole law compresses S2 to ~130ms after S1, so it is suppressed
naturally.

**Three caveats decide what a capture can show:**
- *Compare like windows (and matched loudness).* HF-vs-fundamental ratios depend strongly on how the S1
  is windowed: an **envelope-peak** window (what capture beat-detection yields) and a **systole-onset**
  window (`engine_beats` on an offline render) give different numbers - e.g. offline c=1.0 high-HR reads
  200-450/fund = 0.14 systole-windowed but 0.05 envelope-peak-windowed. A capture-vs-offline comparison
  MUST window both the same way, or a pure windowing difference masquerades as a real gap - this twice
  produced a spurious "contractility deficit" / "offline has more 200-450 Hz" until the offline was
  re-windowed to match the capture. Averaging many beats also smears narrow harmonic peaks (F0 jitter),
  so cross-check per-beat medians. Likewise, since lossy encoders allocate HF bits by loudness, comparing
  two captures at different levels shifts the measured HF - match the loudness or use lossless.
- *Absolute values do not survive lossy (AAC) encoding.* Encoding plus onset-vs-peak windowing shifts
  the absolute centroid ~15%, so it cannot be compared to an offline render's absolute number - only
  **within-signal ratios** (e.g. the rest->peak brightening ratio) and **peak amplitude** transfer, the
  same "ratios only" rule as cross-recording comparison. So a capture confirms *consistency* and *no
  gross regression*, but a DSP fix whose signature is below the ~15% encoding noise cannot be positively
  proven present from a compressed capture (use a lossless WASAPI-loopback capture for that). Tell a real
  regression from an encoding artifact by whether the offset is *drive-dependent* (a real DSP change
  varies with contractility) or *drive-flat* (an artifact).
- *Broadband centroid is the wrong brightness metric for our source.* The magnitude-weighted centroid
  (fmax 1200 Hz) is dominated by the loud low fundamental and our source's 20-40 Hz boom, so it can move
  *opposite* to perceived brightness: adding low-frequency ring energy above the source's sub-40 Hz boom
  can raise the centroid while dulling the beat. For perceived brightness use the **80-200 Hz "octave"**,
  or an A-weighted HF measure.

**The game playback route is spectrally transparent, so core-rendered output transfers faithfully.**
Controlled captures pinned to a known state (fixed HR, contractility, breath off, PVCs off) and standing
still with the in-game sound slider at 0 so the heartbeat is isolated match the corresponding offline
80-200 Hz-to-body ratio to three decimals across rest and high HR at both zero and full contractility.
The active float path also has a lossless, centred, unclipped before/after capture comparison and a
level-matched ear pass across rest, rising and peak drive, inspiration, and recovery. Fixed four-second
windows matched by phase and HR show no drive-dependent 80-200 Hz-to-body offset. PVCs were deliberately
omitted because their sound remains pending retuning, so this is not a perceptual PVC validation.
There is no routing-path brightness attenuation:
brightness tuned to sound right offline transfers in-game, and the source sample (not the playback path)
is the brightness ceiling. The one measurable in-game difference is a small (~3-4%) crest reduction on the
loudest (full-contractility) beats, absent at rest - mild peak-limiting in the game's mastering voice, a
*dynamics* effect, not a timbre one. It follows that an audible before/after difference from a
loudness-affecting change (e.g. a `ContractilityGainDb` retune) is *loudness*, not timbre - recover it with
`Config::Audio::Volume`, not a brightness change.
