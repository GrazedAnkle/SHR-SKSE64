# Roadmap

The current milestone is **evidence-safe audio refinement**: close or precisely characterize the remaining
S2 and breath-transmission gaps without letting an invalid ruler or mismatched physiological state drive
another retune. The settled S1 baseline remains fixed while those independent gaps are evaluated.

The milestone succeeds when the S2 annotation convention is operational, breath spectral claims are
either reproducible or demoted, and every changed coefficient has an auditable ruler, state, and
provenance.

## Current queue

In dependency-aware rough priority order:

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

## Later milestones

### Rhythm and simulation

- [WI-019](https://github.com/GrazedAnkle/SHR-SKSE64/issues/21) lands before
  [WI-010](https://github.com/GrazedAnkle/SHR-SKSE64/issues/14): neighbor-beat tuning needs correct
  compensatory-pause scheduling under it.
- [WI-012](https://github.com/GrazedAnkle/SHR-SKSE64/issues/15) deliberately bundles preload/afterload
  systole hysteresis, exertion response, HR-versus-demand validation, and ventilation kinetics, because
  their interactions have to be tested together rather than tuned as isolated constants.
- Contractility-driver separation and fight-or-flight gameplay remain a later architectural milestone;
  [CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md#deferred-driver-separation) owns contractility v2, including
  replacement of the known v1 adrenaline double route.
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
- The MCM capability chain runs in a fixed order:
  [WI-022](https://github.com/GrazedAnkle/SHR-SKSE64/issues/8) gives the audio voice an owner, then
  [WI-026](https://github.com/GrazedAnkle/SHR-SKSE64/issues/7) settles the thread contract and consolidates
  the adapter's file-scope state into the owner that settings updates are delivered into, then
  [WI-023](https://github.com/GrazedAnkle/SHR-SKSE64/issues/6) defines co-save record validation, and only
  then [WI-034](https://github.com/GrazedAnkle/SHR-SKSE64/issues/4) adds the menu itself - beginning with a
  feasibility spike that decides how much packaging work it carries.

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
