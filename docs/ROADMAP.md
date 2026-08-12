# Roadmap

The current milestone is **contractility v2**
([#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27)): replace the v1 driver model, in which acute
arousal reaches heart rate through the exertion proxy, with separated vagal, sympathetic, and
circulating-catecholamine terms. [CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md#driver-separation) owns the
model; the audio consumers keep their current snapshot boundary throughout.

The milestone is scheduled now because it finally has a ruler. Heart-rate kinetics cannot be validated
against the reference recordings, which capture sound rather than trajectories, and the published
heart-rate-recovery bracket assembled for
[#26](https://github.com/GrazedAnkle/SHR-SKSE64/issues/26) is the first external, non-self-referential
target the driver model can be held to.

The milestone succeeds when post-combat recovery is monotone and inside that bracket, the double route is
gone rather than tuned around, and every constant the new drivers introduce carries a literature entry.

## Current queue

In dependency-aware rough priority order:

1. [#22: bracket the remaining [physio] constants](https://github.com/GrazedAnkle/SHR-SKSE64/issues/22),
   heart-rate-dynamics slice only - v2 re-derives those constants and adds more beside them.
2. [#24: acute and chronic state integrate on different clocks](https://github.com/GrazedAnkle/SHR-SKSE64/issues/24) -
   v2 re-derives the same integrators, so the clock semantics must be settled under it.
3. [#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27) itself, validated by
   [#26](https://github.com/GrazedAnkle/SHR-SKSE64/issues/26).

## Later milestones

### Evidence-safe audio refinement

Parked mid-flight rather than finished. The settled S1 baseline remains fixed, and the queue below resumes
in this order. The open convention question is the one thing not recoverable from these documents: the
annotator's hand-versus-detector S2 onset convention needs a clap-present discriminator before
[WI-016](https://github.com/GrazedAnkle/SHR-SKSE64/issues/18) can use the validated HF-onset heuristic.

1. [WI-005: cross-gap reference-claim audit](https://github.com/GrazedAnkle/SHR-SKSE64/issues/10) -
   classify and remeasure every distant-window reference claim.
2. [WI-008: spectral breath-muffle evidence](https://github.com/GrazedAnkle/SHR-SKSE64/issues/12) -
   separate reproducible muffle evidence from suspect pitch motion.
3. [WI-015: calibration provenance audit](https://github.com/GrazedAnkle/SHR-SKSE64/issues/17) -
   leave no unaudited tuned-constant provenance placeholder.
4. [WI-007: blunt S2 attack](https://github.com/GrazedAnkle/SHR-SKSE64/issues/11) - a sharper S2 envelope
   with fundamental and timing intact.
5. [WI-001: reference breath landmarks](https://github.com/GrazedAnkle/SHR-SKSE64/issues/9) - defensible
   refs 11/14/15 breath landmarks.
6. [WI-009: breath-curve asymmetry](https://github.com/GrazedAnkle/SHR-SKSE64/issues/13) - a smooth
   state-dependent inspiration/expiration curve.

### Rhythm and simulation

- [WI-019](https://github.com/GrazedAnkle/SHR-SKSE64/issues/21) lands before
  [WI-010](https://github.com/GrazedAnkle/SHR-SKSE64/issues/14): neighbor-beat tuning needs correct
  compensatory-pause scheduling under it.
- [WI-012](https://github.com/GrazedAnkle/SHR-SKSE64/issues/15) keeps post-exercise systole and
  ventilation kinetics once [#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27) takes its
  exertion-response and heart-rate-versus-demand items, and waits on it for the sympathetic systole term.
  What still binds the remainder is whole-trajectory audition rather than a shared driver, which is why
  the fatigue time constants left for [#28](https://github.com/GrazedAnkle/SHR-SKSE64/issues/28): they act
  on twenty-minute to multi-day scales that no single audition covers.
- Fight-or-flight gameplay follows [#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27), which is
  what gives it an arousal signal to read without routing gameplay effects through the audio scalar.
- Swimming/breath hold, sitting/resting posture, and over-capacity gameplay consequences wait for the
  dynamics pass or stronger reference evidence.
- Expanded arrhythmia states (AF, SVT, VF, sustained bigeminy) may justify a coroutine-based rhythm
  architecture; the working PVC path alone does not.

### Additional audio realism

- [WI-017](https://github.com/GrazedAnkle/SHR-SKSE64/issues/19) and
  [WI-018](https://github.com/GrazedAnkle/SHR-SKSE64/issues/20) share the existing respiratory envelope;
  design and balance them together, after
  [WI-009](https://github.com/GrazedAnkle/SHR-SKSE64/issues/13) settles the curve they both build on.
- S2 split and the site-dependent P2 valve clap remain deferred until position/site state or a defensible
  default exists.
- S3/S4, murmurs, systolic-rumble characterization, subject age/posture, and alternate saturated sound
  profiles are lower-priority realism or pathology work.
- SFX-submix routing precedes third-person spatial audio; the current direct mastering-voice route remains
  a deliberate interim.
- Vigor-jitter refinements (a resting floor, state-based arousal anchoring, and a larger-sample tail check)
  wait for the envelope milestone and stronger independent state evidence.

### Evidence and engineering maintenance

- [WI-014](https://github.com/GrazedAnkle/SHR-SKSE64/issues/16) needs design before flags, bracket-only
  beats, and transition regimes can be consumed systematically;
  [WI-016](https://github.com/GrazedAnkle/SHR-SKSE64/issues/18) stays deferred until a hand-annotated
  S2-louder straddle supplies ground truth.
- Re-annotating lost prose-only reference windows, catalog cleanup, extreme-value testing, derived-value
  citation support, and prose-consistency invariants are maintenance candidates.
- [#23](https://github.com/GrazedAnkle/SHR-SKSE64/issues/23) separates transient physiological modifiers
  from persisted settings. Contractility v2 raises its priority rather than settling it: an arousal driver
  is exactly the kind of transient state that must not reach the settings layer.

## Untriaged ideas

These are captured only so they are not lost. Promote one to an issue only after its outcome and
dependencies are understood.

- Blood-pounding/whoosh over the whole mix near maximum HR or near death.
- A more explicit "catching your breath" recovery cue.
- Valsalva grunt and brief breath hold on heavy attacks.
- Cold-water gasp followed by a sustained diving-reflex response.
- Injury/hemorrhage tachycardia and weakness.
- Orthostatic HR bump on standing.
- A unified physiological-modifier input pipeline for fear, temperature, potions, injury, and posture.
- Warm-up and cooldown behavior.
