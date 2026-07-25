# Runtime Architecture

SHR separates deterministic heart behavior from Skyrim and audio-device integration. The
Skyrim-independent `shr_core` library owns physiology, rhythm scheduling, per-beat acoustic mapping,
source conditioning, and beat rendering; the plugin translates engine state and effects at the boundary.
This document owns that dependency and runtime boundary. [SIMULATION_MODEL.md](SIMULATION_MODEL.md) owns
physiological state evolution, while [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md) owns the state-to-sound
mechanisms.

## Dependency boundary

Dependencies point inward:

```text
Skyrim adapters                         shr_core
---------------                         --------
RE player state ----> StepInput ------> Runtime
Config / TOML ------> RuntimeSettings    |-- HeartRateSimulation --> PhysiologySnapshot
SKSE events --------> typed events       |-- RhythmEngine --------> optional BeatEvent
co-save records <---> SimulationState    `-- CreateRenderSpec ----> RenderSpec
                                                                  |
 decoded source --------------------------> source conditioning -> beat renderer
                                                                  |
                                                           XAudio sink
```

`shr_core` has no CommonLib, XAudio, TOML, spdlog, or plugin-precompiled-header dependency. Pocketfft is a
private numerical implementation dependency for the analytic envelope; it does not cross the public API.
The plugin and core tests link that same library rather than compiling private copies of its
implementations. The `Core-Release-Clang` preset resolves only portable core dependencies and proves the
boundary without configuring CommonLib or the plugin target.

The active plugin path decodes source PCM into a typed float `HeartbeatSource`, performs the complete
source -> transmission -> transducer chain through `RenderBeat`, then calls `EncodePcm16` exactly once at
the XAudio submission boundary. `TraceBeatRender` retains domain outputs for tests and offline inspection
without burdening normal playback.

## Runtime contract

`Runtime` is the single owner of `HeartRateSimulation` and `RhythmEngine`. A `Step`:

1. advances physiology from `StepInput::Player`, real frame time, and elapsed game time;
2. captures the post-step `PhysiologySnapshot`;
3. derives rhythm and arrhythmia inputs from that snapshot;
4. advances rhythm when output is enabled; and
5. maps a fired `BeatEvent` and the same snapshot into a `RenderSpec`.

`StepResult` always returns the physiology snapshot and optionally pairs the raw beat event with its
resolved render specification. Keeping both values lets notification policy and offline traces inspect
rhythm facts without reconstructing them from renderer controls.

Listening is part of the runtime behavior contract. When `StepInput::OutputEnabled` is false, physiology
continues to advance but rhythm remains frozen. This preserves beat scheduling when heartbeat output is
disabled rather than silently consuming a rhythm sequence.

`Runtime::Init` resets simulation and rhythm together. `SimulationState` is the persistence boundary and
contains only state required to resume the physiological model; rhythm state is not persisted.
`Runtime::Restore` therefore restores that simulation value without interpreting record versions or
missing fields. The Skyrim serialization adapter owns versioned records, validation policy, and
legacy-field defaults.

## Value and responsibility split

- `RuntimeSettings` contains subject/runtime configuration: `SimulationSettings` and arrhythmia
  susceptibility. Model coefficients remain auditable defaults in `Constants.hpp`; a future offline
  coefficient-override API is a separate immutable value rather than an expansion of runtime settings.
- `PhysiologySnapshot` is the cohesive downstream view of current physiological and derived state.
  `SimulationState` is the complete resumable state.
- `RhythmInput` carries the values required to schedule a beat. `BeatEvent` owns scheduled-beat facts:
  kind, interval and coupling/filling timing, and sampled per-beat vigor.
- `CreateRenderSpec` owns the conversion from an event plus its firing snapshot into amplitudes, systole,
  bounded Frank-Starling gain, onset compression, and respiratory transmission controls.
- Core source conditioning slices the decoded asset, applies its static high-pass and joint
  normalization, and stores owned normalized-stereo S1/S2 plus the baseline S1 attack region for the
  float renderer. `RenderBeat` consumes that source and a `RenderSpec` without filesystem, device, or
  Skyrim state. The plugin owns WAV container parsing; offline clients may supply the same decoded
  samples without reproducing conditioning or rendering.
- The Skyrim adapter owns RE/SKSE mapping, game-clock sampling, event delivery, co-save translation, HUD
  policy, pause/resume integration, WAV/file I/O, and XAudio submission. The audio sink owns device
  volume and queue/resource behavior.

Core event ingress is synchronous and typed. Whether an SKSE callback can forward directly or requires a
single-writer mailbox remains a thread-contract decision under
[WI-026](work_items/WI-026-runtime-thread-contract.md).

## Offline execution

`shr_core` is the single implementation of physiology, rhythm, acoustic mapping, source conditioning, and
beat rendering for both the plugin and offline analysis; Python reimplements none of them. The binding in
`bindings/shr_pybind.cpp` compiles that same target into a Python extension so scenario, analysis, and
reporting code drives the compiled core directly. It builds under the `BUILD_PYBIND` option against the
analysis virtual environment's Python, with no CommonLib, Skyrim, or XAudio dependency (see the README build
steps). WAV container parsing stays in Python because the plugin owns it in-game, so the binding accepts
decoded PCM16 and returns NumPy float audio alongside physiology snapshots, beat events, and render
specifications.

The binding exposes `Runtime` for scripted `StepInput` scenarios and notify events, `RhythmEngine` and
`CreateRenderSpec` for direct-input rhythm and mapping, and the source-conditioning and `RenderBeat` path.
Determinism comes from a seeded `RhythmRandom` that reuses production's per-call distributions, so a seed
reproduces the plugin's draw math; `HeartRateSimulation` is otherwise deterministic.

Committed golden manifests under `tests/golden/` pin core output for source conditioning, beat rendering,
rhythm and mapping, and full trajectories. Each is captured from the compiled core through the `shr_pybind`
binding and re-verified by its golden-check tool, which regenerates the manifest only on an intentional
retune. These goldens are the permanent regression anchor for offline core behavior, and deterministic offline
scenarios are the primary acceptance gate for physiology, rhythm, and DSP changes. Python owns scenario
construction, measurement, annotation, and reporting as independent analysis rulers, not as production
mirrors. SKSE input mapping, XAudio ownership and scheduling, thread delivery, and game-mix behavior retain
their own in-game gates.
