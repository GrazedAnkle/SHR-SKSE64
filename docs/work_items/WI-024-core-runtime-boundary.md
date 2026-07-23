# WI-024: Core and Runtime Boundary

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Physiology, rhythm scheduling, acoustic state mapping, and their tests build as a Skyrim-independent core
target. A single owned runtime object coordinates that core; SKSE hooks, events, serialization, HUD calls,
and XAudio are thin adapters that depend inward. Tests link the same core objects shipped by the plugin
rather than recompiling selected implementation files.

The boundary exposes cohesive values for step input, physiology snapshots, persisted simulation state,
rhythm input, beat events, and render specifications. CommonLib types and process-global configuration do
not cross into the core API.

## Current conclusion

The conceptual model already distinguishes simulation, rhythm, cardiac-source mapping, transmission,
transducer, and game integration. The C++ boundary is less explicit: `SkyrimHeartRate.cpp` owns anonymous
global subsystems while also handling hooks, co-saves, player mapping, arrhythmia policy, notifications,
and playback. `RhythmEngine::Beat` mixes rhythm facts with audio amplitudes, simulation reads the global
`Config`, and the test executable recompiles `Config.cpp` and `Simulation.cpp` instead of linking a core
library.

The target shape is one-way data flow:

`SKSE input -> runtime -> physiology snapshot -> rhythm event -> acoustic render spec -> audio sink`,

with persistence translating separately between SKSE records and a versioned simulation-state value.

## Scope and non-goals

Create ownership and dependency boundaries without changing physiology, rhythm timing, DSP coefficients,
or audible output. Keep aggregate models where their state evolves together; do not split
`HeartRateSimulation` into microclasses merely to reduce file length. Do not perform a repository-wide
naming rewrite or introduce abstract interfaces where a concrete value/function boundary suffices.

The float DSP representation is [WI-025](WI-025-typed-float-renderer.md), audio resource lifetime is
[WI-022](WI-022-audio-resource-ownership.md), and runtime thread delivery is
[WI-026](WI-026-runtime-thread-contract.md). The compiled offline consumer and retirement of the Python
behavioral mirrors belong to [WI-027](WI-027-unified-offline-execution.md).

## Dependencies

- [WI-019](WI-019-pvc-compensatory-pause.md) already requires deterministic random draws and is a useful
  first rhythm seam, but the core target need not wait for the timing correction.

## Next action and decision points

Write a target/dependency sketch and extract the smallest behavior-neutral `shr_core` library first.
Introduce an immutable settings value in place of core calls to `Config::Get`, then make the existing tests
link that target. Next add `RhythmInput`, injected/scriptable randomness, and a distinct beat-event value;
move amplitude mapping only after parity tests capture the current calculation. The maintainer approves the
names and the exact rhythm-versus-acoustic boundary before files are moved broadly.
