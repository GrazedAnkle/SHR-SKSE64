# Runtime Architecture

SHR separates deterministic heart behavior from Skyrim and audio-device integration. The
Skyrim-independent `shr_core` library owns physiology, rhythm scheduling, per-beat acoustic mapping,
source conditioning, and beat rendering; the plugin translates engine state and effects at the boundary.
This document owns that dependency and runtime boundary. [SIMULATION_MODEL.md](SIMULATION_MODEL.md) owns
physiological state evolution, while [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md) owns the state-to-sound
mechanisms.

## Dependency boundary

Three source directories hold three populations with different dependency rules, and each build
target lists exactly one of them, so a file's directory states which target owns it:

| Directory | Target | May reach |
|---|---|---|
| `src/core` | `shr_core` | nothing beyond the standard library and private pocketfft |
| `src/adapter` | `shr_adapter` | `src/core`, spdlog, toml11 |
| `src/plugin` | `SHR` | `src/adapter`, `src/core`, CommonLibSSE, XAudio |

`src/` is the single include root for every target, so an include names the layer it reaches into -
`#include "core/RenderSpec.hpp"` - and each crossing is visible where it happens. The permitted
direction is downward only. `tools/check_layering.py` enforces that direction, the qualification
itself, and that each layer directory and its CMake source list hold the same files; the last of
those is what keeps a new file from being silently omitted from a target.

Shared value types - `RenderSpec`, `BeatEvent`, `PhysiologySnapshot`, `SimulationState` - live in
`src/core` rather than a separate shared directory. They are core-owned outputs, and a shared
directory's rule would be "anyone may depend on this", which is weaker than the downward-only chain
rather than stronger. Layers are flat rather than component trees with public `include/` directories.
That shape would make the direction compiler-enforced, since per-component include directories
propagate only along link edges, but the include strings are identical either way, the private-header
set that would justify it is one file (`Random.hpp`), and the preset and checker below already cover
the direction. Promoting `src/core/*.hpp` to `src/core/include/core/*.hpp` later would therefore not
touch a single include.

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
boundary without configuring CommonLib or the plugin target: a core file reaching upward pulls a library
that preset does not supply, so the crossing fails to configure. That leaves header-only crossings, which
would compile on either side of the boundary, and `tools/check_layering.py` covers those.

`shr_adapter` is the Skyrim-free half of the adapter: configuration parsing, logging configuration, and
notification message selection. These are adapter policy rather than model behavior, so they stay out of
`shr_core` even though they reach no Skyrim API - the core's dependency rule is what makes it portable.
Logging is split along that line: `adapter/LoggingConfiguration.hpp` declares the level application, which
touches only the installed default logger, while `plugin/Logging.hpp` declares the sink installation that
resolves the SKSE log directory.

The Catch2 suite is gated on that same boundary. The Windows native-tests workflow configures the
plugin-free `Core-Tests-Release-MSVC` preset, whose vcpkg feature set supplies Catch2, spdlog, and toml11
without the plugin dependency closure, and runs the suite through its `Core-Unit-Tests` test preset. The
test executable links `shr_adapter`, which carries `shr_core` and spdlog with it, and compiles no `src/`
sources of its own, so it needs neither the CommonLib submodule nor a Skyrim module. Nothing in the suite
hashes floating-point bytes, so unlike the golden gate it pins no compiler.

Every test in the suite is a unit test, so both test presets run all of them and neither filters.
`catch_discover_tests` is called with `ADD_TAGS_AS_LABELS`, so each Catch2 tag is a CTest label and
`ctest -L rhythm` selects by subject ad hoc. No preset is built on those labels: the tag vocabulary names
what a test is *about* rather than what it *needs to run*, and only the latter can justify excluding a test
from a gate. The first test requiring a Skyrim module or a running engine introduces that vocabulary and
brings its excluding preset with it; a filter written earlier selects nothing, which CTest reports as an
error rather than an empty run.

The project's CMake options are `SHR_`-prefixed so they cannot alias an identically named option in a
dependency added through `add_subdirectory`.

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
  susceptibility. `ModelCoefficients` is a separate immutable calibration value with typed `Simulation`,
  `Rhythm`, `AcousticMapping`, `SourceConditioning`, and `BeatRendering` groups.
  `ModelCoefficientRegistry.hpp` is the single structural catalog of each live field's stable name,
  scalar type, and group; it generates the typed members and the binding descriptors used for both
  override lookup and value reporting. The same catalog explicitly classifies non-live constants as
  derived, asset-fixed, utility, game-integration, or dormant. `Constants.hpp` remains the auditable
  production-default and provenance index, and `tools/check_constants.py` requires exact name/type
  coverage between the two. A runtime retains one aggregate and copies the relevant group into each
  long-lived behavioral owner; coefficient-free C++ entry points delegate to the same immutable
  production default.
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
- The plugin layer owns RE/SKSE mapping, game-clock sampling, event delivery, co-save translation, HUD
  policy, pause/resume integration, WAV/file I/O, and XAudio submission. The audio sink owns device
  volume and queue/resource behavior.

Core event ingress is synchronous and typed. Whether an SKSE callback can forward directly or requires a
single-writer mailbox remains a thread-contract decision under
[WI-026](work_items/WI-026-runtime-thread-contract.md).

## Offline execution

`shr_core` is the single implementation of physiology, rhythm, acoustic mapping, source conditioning, and
beat rendering for both the plugin and offline analysis; Python reimplements none of them. The binding in
`bindings/shr_pybind.cpp` compiles that same target into a Python extension so scenario, analysis, and
reporting code drives the compiled core directly. It builds under the `SHR_BUILD_PYBIND` option against the
analysis virtual environment's Python, with no CommonLib, Skyrim, or XAudio dependency (see the README build
steps). WAV container parsing stays in Python because the plugin owns it in-game, so the binding accepts
decoded PCM16 and returns NumPy float audio alongside physiology snapshots, beat events, and render
specifications.

The binding exposes `Runtime` for scripted `StepInput` scenarios and notify events, `RhythmEngine` and
`CreateRenderSpec` for direct-input rhythm and mapping, and the source-conditioning and `RenderBeat` path.
Determinism comes from a seeded `RhythmRandom` that reuses production's per-call distributions, so a seed
reproduces the plugin's draw math; `HeartRateSimulation` is otherwise deterministic.

Offline coefficient sweeps use the bound immutable `ModelCoefficients` value. Python starts from
`default_model_coefficients`, reports the selected `values`, applies a named batch with
`with_overrides`, and passes the resulting aggregate consistently to each operation in a run. The binding
descriptors preserve registry scalar type: structural controls remain integers, coordinated knots or
bounds validate only after the complete batch is applied, and unsupported derived, asset, utility,
dormant, or game-integration names fail with their reason. Generic per-field finiteness checks come from
the same registry; cross-field and consuming-context validation remains explicit beside the formulas it
protects. Core formulas and global state are never reconstructed or mutated by the registry.

Committed golden manifests under `tests/golden/` pin core output for source conditioning, beat rendering,
rhythm and mapping, and full trajectories. Each is captured from the compiled core through the `shr_pybind`
binding and verified as one pytest case per domain. Verification and authoring are separate entry points
by one verb each: `tools/golden_registry.py` enumerates the domains and the uniform surface each exposes,
pytest verifies them, and `tools/capture_goldens.py` is the only path that writes a manifest. Nothing
reachable from a test run can rewrite the values it is checking, because a verify that passes immediately
after a recapture proves only that the manifest was just overwritten; the reviewable manifest diff is what
establishes an intended retune. Each domain's inputs are defined once, beside the domain that consumes
them: the named beat-render operating points in `tools/beat_render_fixtures.py`, and the rhythm, mapping,
and trajectory scenarios in their sibling modules. The C++ suite asserts renderer and engine properties
from its own local inputs rather than mirroring those tables, so there is no second definition that can
silently disagree, and every fixture change surfaces as a manifest diff. The immutable
coefficient-override contract is verified in the same suite, as assertions rather than a manifest. The Windows offline-goldens workflow builds the portable binding with
the pinned capture compiler and runs the pytest suite, independently of the CommonLib/plugin build. The compiler pin matters because the waveform gates hash raw float bytes. These
goldens are the permanent regression anchor for offline core behavior, and deterministic offline scenarios
are the primary acceptance gate for physiology, rhythm, and DSP changes. Python owns scenario construction,
measurement, annotation, and reporting as independent analysis rulers, not as production mirrors. SKSE input
mapping, XAudio ownership and scheduling, thread delivery, and game-mix behavior retain their own in-game
gates.

Offline audition files preserve the compiled renderer's native channel layout; the current source and
renderer output are stereo. Numerical analysis selects channel zero explicitly, matching reference-tool
loading and the baseline S1 attack locator, rather than implicitly downmixing a native-layout render.
`tools/audition_core.py` is the canonical audition client for this contract. `tools/rhythm_offline.py`
and `tools/sim_offline.py` retain their steady-state measurement and trajectory-reporting surfaces as
thin clients over `RhythmEngine`, `Runtime`, acoustic mapping, and rendering. Their `--set` options build
one immutable coefficient aggregate and pass it through the complete run.

Retired late-S1 tail/tamer experiments are isolated in `tools/legacy_s1.py`. They consume the compiled
trace's post-onset source stage and return the altered stage to `RenderBeatFromSourceStages`, so active
transmission, resampling/mixing, and limiting remain core-owned even in a counterfactual audition. These
transforms are reproducibility fixtures for the purpose of documentation.
