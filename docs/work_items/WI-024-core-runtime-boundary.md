# WI-024: Core and Runtime Boundary

Status: `[ACTIVE]`

## Outcome and acceptance criteria

Physiology, rhythm scheduling, acoustic state mapping, and their tests build as a Skyrim-independent core
target. A single owned runtime object coordinates that core; SKSE hooks, events, serialization, HUD calls,
and XAudio are thin adapters that depend inward. Tests link the same core objects shipped by the plugin
rather than recompiling selected implementation files.

The boundary exposes cohesive values for step input, physiology snapshots, persisted simulation state,
rhythm input, beat events, and render specifications. CommonLib types and process-global configuration do
not cross into the core API. Candidate names are `StepInput`, `PhysiologySnapshot`, `SimulationState`,
`RhythmInput`, `BeatKind`, `BeatEvent`, and `RenderSpec`; the names and exact field ownership remain
provisional until each seam is implemented.

A core-only configure and build path does not require CommonLib, XAudio, TOML, spdlog, pocketfft, or the
plugin precompiled header. The plugin and later offline consumer link the same `shr_core` target. Existing
physiology trajectories, rhythm timing, random distributions, acoustic mapping, and audible output remain
unchanged across the extraction.

## Current conclusion

The conceptual model already distinguishes simulation, rhythm, cardiac-source mapping, transmission,
transducer, and game integration. The C++ boundary is less explicit: `SkyrimHeartRate.cpp` owns anonymous
global subsystems while also handling hooks, co-saves, player mapping, arrhythmia policy, notifications,
and playback. `RhythmEngine::Beat` mixes rhythm facts with audio amplitudes, simulation reads the global
`Config`, and the test executable recompiles `Config.cpp` and `Simulation.cpp` instead of linking a core
library.

The build boundary is also incomplete before any source extraction: top-level CMake configures CommonLib
and finds plugin, configuration, and renderer dependencies unconditionally. Adding a library target
without a plugin-off path would therefore leave offline and core-only consumers Skyrim-dependent at
configure time.

Listening is presently part of behavior, not merely presentation: `HandleFeedback` does not advance
`RhythmEngine` while listening is disabled. The initial core runtime must preserve that freeze through an
explicit, game-agnostic output-enabled input. Continuously advancing a silent rhythm may be considered
later as a deliberate behavior change, but is not part of this extraction.

The target shape is inward-only, one-way data flow:

`SKSE input -> runtime -> physiology snapshot -> rhythm event -> acoustic render spec -> audio sink`,

with persistence translating separately between SKSE records and a versioned simulation-state value.

## Provisional target and responsibility sketch

```text
Skyrim adapters                         shr_core
---------------                         --------
RE player state ----> step input -----> runtime
Config / TOML ------> settings            |-- simulation ----> physiology snapshot
SKSE events --------> typed events        |-- rhythm --------> optional beat event
co-save records <---> simulation state    `-- acoustic map --> render specification
                                                                  |
                                                        XAudio / offline renderer
```

- The core owns physiology state evolution, rhythm state and scheduling, per-beat acoustic mapping, and
  the orchestration that passes values between them.
- Skyrim adapters own RE/SKSE value mapping, event delivery, co-save record encoding and validation, HUD
  presentation, pause/resume calls, and XAudio playback.
- The audio sink owns output volume and queue/resource behavior. The render specification owns cardiac
  and transmission inputs that affect the generated beat, not device or game-mix policy.
- `BeatEvent` should describe a scheduled sinus or PVC event and its rhythm facts. S1/S2 gains and other
  morphology currently mixed into `RhythmEngine::Beat` should move to the acoustic render specification
  only after characterization tests protect the calculation. Whether systole and sampled per-beat vigor
  are event facts or acoustic-map results remains an explicit decision point.
- A runtime step returns a physiology snapshot and an optional beat event/render specification when a
  beat fires. It does not send HUD messages or submit audio itself.
- The persisted simulation-state value contains only state needed to resume the model. SKSE record
  versions, missing-field defaults, and malformed-record policy stay in the serialization adapter under
  [WI-023](WI-023-cosave-record-validation.md).

Runtime/subject settings and model coefficients are separate concerns. The former replace core reads of
the process-global `Config`; the latter remain the auditable defaults in `Constants.hpp`. Offline
coefficient overrides eventually need an immutable parameter value, but moving every constant is not a
prerequisite for the first core extraction and must not silently break the constant-provenance and
data-citation tools.

## Scope and non-goals

Create ownership and dependency boundaries without changing physiology, rhythm timing, DSP coefficients,
or audible output. Keep aggregate models where their state evolves together; do not split
`HeartRateSimulation` into microclasses merely to reduce file length. Do not perform a repository-wide
naming rewrite or introduce abstract interfaces where a concrete value/function boundary suffices.

The float DSP representation is [WI-025](WI-025-typed-float-renderer.md), audio resource lifetime is
[WI-022](WI-022-audio-resource-ownership.md), and runtime thread delivery is
[WI-026](WI-026-runtime-thread-contract.md). The compiled offline consumer and retirement of the Python
behavioral mirrors belong to [WI-027](WI-027-unified-offline-execution.md).

Core event handling is synchronous and typed; WI-026 decides whether an adapter can call it directly or
must drain a mailbox. This item does not remove notification atomics until that delivery contract is
known. Randomness becomes injectable and scriptable at the rhythm seam, but exact RNG-state persistence
is not required. Coarse-step multi-beat draining and changes to the current one-beat-per-update behavior
are also outside the behavior-neutral extraction.

## Dependencies

- [WI-019](WI-019-pvc-compensatory-pause.md) already requires deterministic random draws and is a useful
  first rhythm seam, but the core target need not wait for the timing correction.

## Incremental extraction

1. Capture a deterministic rest-to-exercise-to-recovery simulation trajectory and time-skip checkpoints
   as behavior-neutral characterization fixtures.
2. Create the smallest `shr_core` static library around `HeartRateSimulation`, inject its resting and
   maximum-HR settings, and make the plugin and simulation tests link it. Add a plugin-off configure/build
   path proving that this target has no Skyrim or renderer dependency.
3. Replace the scalar restore call and scattered getters with cohesive simulation-state and physiology
   snapshot values, retaining current legacy-save defaults in the SKSE adapter.
4. Add rhythm characterization tests, a `RhythmInput`, `BeatKind`, injected/scriptable randomness, and an
   optional beat-event result. Preserve random distributions and the current listening gate.
5. Move acoustic amplitude and morphology mapping out of the rhythm event only after parity fixtures
   capture the present sinus, PVC, post-pause, inspiration, and extreme-vigor calculations.
6. Introduce the owned runtime that composes the extracted objects, then reduce `SkyrimHeartRate.cpp` to
   adapter orchestration and integration policy.

## Next action and decision points

Implement steps 1 and 2 as the first reviewable slice. Before step 3, approve the exact state/snapshot
names and whether legacy optionality belongs in the core state value or only in the serialization adapter.
Before steps 4 and 5, approve the rhythm-versus-acoustic ownership of systole, Frank-Starling preload, and
sampled vigor. Decide the long-term runtime/subject-settings versus model-parameter API before WI-027
exposes coefficient overrides to Python.
