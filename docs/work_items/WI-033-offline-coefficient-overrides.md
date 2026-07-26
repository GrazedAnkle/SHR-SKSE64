# WI-033: Immutable Offline Coefficient Overrides

Status: `[NEEDS DESIGN]`

## Outcome and acceptance criteria

Offline clients can sweep selected model coefficients without rebuilding the binding, while production
continues to use one immutable default coefficient set. Python selects and reports overrides but
reimplements no physiology, rhythm, mapping, source-conditioning, or rendering formula.

The implementation must:

- introduce a typed immutable coefficient value, grouped by simulation, rhythm, acoustic mapping, source
  conditioning, and beat rendering;
- keep that value separate from `RuntimeSettings`, which remains subject/runtime configuration;
- carry one selected value consistently through `Runtime`, `HeartRateSimulation`, `RhythmEngine`,
  `CreateRenderSpec`, `PrepareHeartbeatSource`, and `RenderBeat`;
- expose validated named overrides through `shr_pybind`, returning a new value rather than mutating a
  live runtime or global state;
- preserve the useful `--set Name=value` CLI surface as a thin binding client;
- reject unknown names, wrong scalar types, invalid domains, and unsupported derived/asset/integration
  controls with actionable errors; and
- leave every default golden and core test unchanged, ideally bit-for-bit for rendered output.

## Current conclusion

A binding-only dictionary cannot provide honest overrides because coefficients are read directly in
`Simulation.cpp`, `Runtime.cpp`, `RhythmEngine.cpp`, `AcousticMapper.cpp`, `HeartbeatSource.cpp`, and
`BeatRenderer.cpp`. The coefficient value must therefore be a core dependency carried to every behavioral
owner. Putting it in `RuntimeSettings` would conflate model calibration with subject configuration and
contradict the boundary owned by [ARCHITECTURE.md](../ARCHITECTURE.md).

The existing flat `Constants.hpp` namespace contains several different kinds of value: independently
sweepable floats, integer structural controls, derived values such as `FitnessAbsoluteMin`, fixed asset
landmarks, unit conversions, and engine-integration gain. Treating all of them as generic mutable floats
would erase useful type and dependency constraints.

## Scope and non-goals

Design and implement the immutable coefficient value, default construction, validation, core propagation,
binding surface, and thin CLI mapping needed by WI-028. Keep `Constants.hpp` as the auditable default
parameter index unless the accepted design identifies a single-source alternative that still works with
the existing constant and citation gates.

Do not expose model coefficients as user-facing game configuration, support live mutation of an existing
runtime/source, retune any value, or turn asset landmarks and unit conversions into casual sweep knobs.

## Dependencies

None. WI-028 depends on this item because `sim_offline.py`, `rhythm_offline.py`, and the legacy audition
tools currently preserve `--set`.

## Next action and decision points

Produce a header-level API sketch and settle:

- whether callers own one aggregate `ModelCoefficients` or smaller immutable values passed independently;
- which `Constants.hpp` entries are independently sweepable, derived from other entries, or intentionally
  fixed;
- how derived values are recomputed and whether attempts to override them are rejected;
- whether default-valued overloads remain for focused core tests and plugin call sites;
- where cross-field validation occurs, including errors for invalid knot ordering and zero denominators;
  and
- how the binding maps stable CLI names to typed fields without making strings part of the C++ core API.

## Newly observed work to split out

None.
