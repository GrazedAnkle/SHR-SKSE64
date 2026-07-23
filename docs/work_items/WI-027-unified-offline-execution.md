# WI-027: Unified Offline Execution

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

The same `shr_core` implementation shipped by the plugin drives deterministic offline physiology, rhythm,
acoustic mapping, and beat rendering. Python calls that compiled core and remains the scenario,
analysis, UI, and reporting layer; it no longer reimplements production state evolution, beat scheduling,
morphology, or DSP.

Scripted scenarios run without Skyrim or XAudio and return float audio plus the physiology snapshots, beat
events, and render specifications needed to locate and explain every output beat. Rest, peak,
rest-to-exercise-to-recovery, inspiration, and PVC fixtures exercise both direct-state and full-trajectory
paths. System/acceptance tests can override immutable settings, inject deterministic random draws, inspect
stage outputs where WI-025 exposes them, and distinguish a core failure from an adapter-only integration
failure.

`tools/engine_offline.py`, `tools/rhythm_offline.py`, and `tools/sim_offline.py` are retired or reduced to
thin core clients once their callers migrate. Core physiology, rhythm, and DSP changes require no matching
Python port. In-game checks are optional corroboration for core behavior; SKSE input mapping, XAudio
ownership and scheduling, thread delivery, and game-mix behavior retain their own integration gates.

## Current conclusion

The project currently has three Python behavioral mirrors. `engine_offline.py` reproduces the
`HeartbeatVoice::Play` signal chain, `rhythm_offline.py` reconstructs a steady-state sinus subset, and
`sim_offline.py` ports `HeartRateSimulation::Step`. They enable productive offline work, but every core
behavior change either needs a second implementation or leaves the offline result incomplete.

The paths are conceptually aligned but do not share execution. The engine works on stereo PCM16 buffers
with repeated intermediate quantization, while the renderer mirror works on one mono channel in NumPy
floating point. The engine consumes exact per-beat timing and morphology from `RhythmEngine`; the offline
sequence layer reconstructs a subset and uses a different random generator. The golden comparison in
[WI-025](WI-025-typed-float-renderer.md) is therefore a migration guard, not a desirable permanent
architecture.

The inward-only target and cohesive values in [WI-024](WI-024-core-runtime-boundary.md), plus the pure float
renderer in WI-025, provide the required seams. A narrow compiled Python binding is the current preferred
consumer because the analysis stack already operates on NumPy arrays and needs efficient batched renders
and intermediate inspection. A batched C ABI or process protocol remains a design alternative if it can
preserve that workflow without making the plugin DLL itself an offline dependency.

## Scope and non-goals

Expose the core's immutable settings, scripted state/event inputs, deterministic randomness, direct render
specifications, scenario execution, structured traces, float audio, and sink-exact PCM conversion to the
offline layer. Build the binding or runner from the same core target as the plugin, with no CommonLib,
Skyrim process, or XAudio requirement. Preserve Python measurement and annotation code; those are
independent analysis rulers rather than production-model mirrors.

Migrate callers incrementally, keeping compatibility wrappers where they reduce disruption, then delete
duplicated behavioral code. Do not port NumPy/SciPy metrics, plotting, reference analysis, annotation, or
report generation to C++. Do not retune physiology, rhythm, morphology, filters, envelopes, amplitudes, or
limiter behavior as part of the cutover. Do not make offline success stand in for adapter-specific
ownership, threading, mix, or event-delivery validation.

## Dependencies

- [WI-024](WI-024-core-runtime-boundary.md) supplies the Skyrim-independent target, settings, snapshots,
  beat events, render specifications, and deterministic rhythm seam.
- [WI-025](WI-025-typed-float-renderer.md) supplies the pure renderer, typed buffers, stage tests, and
  one-time PCM conversion boundary.
- Coordinate sink-exact output with [WI-022](WI-022-audio-resource-ownership.md) and the division between
  core and runtime integration tests with [WI-026](WI-026-runtime-thread-contract.md); neither blocks the
  initial offline core consumer.

## Next action and decision points

After the WI-024 target sketch exists, define one offline API around a batch of direct render
specifications and one scripted full-runtime scenario. Capture the current rest, peak, recovery,
inspiration, PVC, truncation, and extreme-vigor fixtures before replacing a mirror. Choose the compiled
access mechanism by prototyping NumPy ownership, settings overrides, deterministic draws, structured beat
metadata, stage traces, and Release-Clang build/discovery; prefer a direct binding unless a narrower bridge
meets those needs cleanly.

Cut over in layers: source conditioning and beat rendering first, then acoustic mapping and rhythm, then
simulation trajectories. At each layer, move all callers and acceptance fixtures before deleting the
corresponding Python implementation. The maintainer approves the public settings/trace surface and the
final list of adapter behaviors that still require an in-game gate.
