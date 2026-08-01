# Roadmap

The current milestone is **evidence-safe audio refinement**: close or precisely characterize the remaining
S2 and breath-transmission gaps without letting an invalid ruler or mismatched physiological state drive
another retune. The settled S1 baseline remains fixed while those independent gaps are evaluated.

The milestone succeeds when the S2 annotation convention is operational, breath spectral claims are
either reproducible or demoted, and every changed
coefficient has an auditable ruler, state, and provenance. Status meanings are defined in
[DOCUMENTATION_CONVENTIONS.md](DOCUMENTATION_CONVENTIONS.md#status-tags).

## Current queue

Rows are in dependency-aware rough priority order. Each linked work item owns its acceptance criteria,
evidence links, and decision points.

| ID | Outcome | Status | Blocker or dependency | Next action |
|---|---|---|---|---|
| [WI-005](work_items/WI-005-cross-gap-audit.md) | Every distant-window reference claim classified and remeasured | `[NEXT]` | None | Rebuild ref11 comparisons on one window rule |
| [WI-008](work_items/WI-008-spectral-breath-muffle.md) | Reproducible muffle evidence separated from suspect pitch motion | `[NEXT]` | WI-005; better reference may be needed | Validate F0 rulers on synthetic muffled signals |
| [WI-015](work_items/WI-015-calibration-provenance.md) | No unaudited tuned-constant provenance placeholders | `[NEXT]` | State/tuning items for some constants | Audit `[[uncited]]` entries feature by feature |
| [WI-007](work_items/WI-007-blunt-s2-attack.md) | A sharper S2 envelope with fundamental and timing intact | `[NEXT]` | None | Regenerate S2 rise targets on the settled onset convention |
| [WI-001](work_items/WI-001-reference-breath-landmarks.md) | Defensible refs 11/14/15 breath landmarks | `[DEFERRED]` | More S1/S2 annotation is required for enough cycles | Resume with one manual beat-grid extension pass |
| [WI-009](work_items/WI-009-breath-curve-asymmetry.md) | Smooth state-dependent inspiration/expiration curve | `[BLOCKED]` | Defensible resting inspiration-fraction anchor | Acquire or derive the resting anchor, then choose driver/kinetics |

## Later milestones

### Rhythm and simulation

- [WI-010: PVC tuning](work_items/WI-010-pvc-tuning.md) is deferred behind the compensatory-pause fix and
  PVC-parity fixture work; the baseline sinus envelope is settled.
- [WI-019: PVC compensatory-pause scheduling](work_items/WI-019-pvc-compensatory-pause.md) removes an
  extra normal interval after single PVCs and runs; land it before WI-010 neighbor-beat tuning.
- [WI-012: bundled simulation/dynamics](work_items/WI-012-simulation-dynamics.md) groups preload/afterload
  systole hysteresis, exertion response, HR-versus-demand validation, and ventilation kinetics so their
  interactions are tested together.
- Contractility-driver separation and fight-or-flight gameplay remain a later architectural milestone;
  [CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md#deferred-driver-separation) owns contractility v2, including
  replacement of the known v1 adrenaline double route.
- Swimming/breath hold, sitting/resting posture, and over-capacity gameplay consequences wait for the
  dynamics pass or stronger reference evidence.
- Expanded arrhythmia states (AF, SVT, VF, sustained bigeminy) may justify a coroutine-based rhythm
  architecture; the working PVC path alone does not.

### Additional audio realism

- [WI-017: breath-sound layer](work_items/WI-017-breath-sound-layer.md) and
  [WI-018: breath-to-breath variability](work_items/WI-018-breath-variability.md) share the existing
  respiratory envelope; design and balance them together after WI-009.
- S2 split and the site-dependent P2 valve clap remain deferred until position/site state or a defensible
  default exists.
- S3/S4, murmurs, systolic-rumble characterization, subject age/posture, and alternate saturated sound
  profiles are lower-priority realism or pathology work.
- SFX-submix routing precedes third-person spatial audio; the current direct mastering-voice route remains
  a deliberate interim.
- Vigor-jitter refinements (a resting floor, state-based arousal anchoring, and a larger-sample tail check)
  wait for the envelope milestone and stronger independent state evidence.

### Evidence and engineering maintenance

- [WI-014: annotation domain/regime validity](work_items/WI-014-annotation-validity.md) needs design before
  flags, bracket-only beats, and transition regimes can be consumed systematically.
- [WI-016: site-change detection](work_items/WI-016-site-change-detection.md) remains deferred until one
  S2-louder base/pulmonary straddle supplies ground truth.
- Re-annotating lost prose-only reference windows, catalog cleanup, extreme-value testing, derived-value
  citation support, and prose-consistency invariants are maintenance candidates.
- [WI-022: audio resource ownership](work_items/WI-022-audio-resource-ownership.md) makes source-voice and
  submitted-buffer lifetime explicit, including failed submission and shutdown.
- [WI-023: co-save record validation](work_items/WI-023-cosave-record-validation.md) defines malformed and
  future-version fallback before changing the persisted-state representation.
- [WI-026: runtime thread contract](work_items/WI-026-runtime-thread-contract.md) determines whether event
  callbacks can forward directly or require a single-writer mailbox, and consolidates the adapter's
  file-scope state into the one owner that settings updates are delivered into.
- [WI-034: MCM capability](work_items/WI-034-mcm-capability.md) adds in-game editing for selected
  subject/runtime and adapter settings. Its persistence scope, override semantics, and live-application
  rules are settled; a feasibility spike decides how much packaging work it carries. It runs after
  WI-022, WI-026, and WI-023, in that order.

## Untriaged ideas

These are captured only so they are not lost. Promote one to a focused work item only after its outcome and
dependencies are understood.

- Blood-pounding/whoosh over the whole mix near maximum HR or near death.
- A more explicit "catching your breath" recovery cue.
- Valsalva grunt and brief breath hold on heavy attacks.
- Cold-water gasp followed by a sustained diving-reflex response.
- Injury/hemorrhage tachycardia and weakness.
- Orthostatic HR bump on standing.
- A unified physiological-modifier input pipeline for fear, temperature, potions, injury, and posture.
- Warm-up and cooldown behavior.
- Random draw from pool of notification messages.
