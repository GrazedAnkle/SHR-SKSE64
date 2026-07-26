# WI-033: Immutable Offline Coefficient Overrides

Status: `[NEXT]`

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

Callers own one immutable `ModelCoefficients` value with typed `Simulation`, `Rhythm`,
`AcousticMapping`, `SourceConditioning`, and `BeatRendering` groups. `Runtime` retains that aggregate and
passes scoped values to the simulation and rhythm owners; the mapping, source-conditioning, and rendering
functions accept only their corresponding group. Small group values are copied into long-lived owners
rather than retained by reference. Existing coefficient-free constructors and functions remain as default
overloads and delegate to one immutable production default.

The aggregate's groups are const after construction. A C++ caller can copy one group, edit the typed
temporary, and construct a validated replacement aggregate; no core API accepts a string name. The
binding owns the stable string-to-field registry and exposes a read-only default value plus a batch
`with_overrides` operation. It applies all requested changes to temporaries, validates the final
combination once, and returns a new value so coordinated knot or bound changes do not fail on an
irrelevant intermediate state. Offline clients construct one value and pass it consistently to every
binding operation.

The override policy is:

- live physiological and DSP coefficients are sweepable;
- live structural controls such as `PVCRunMaxLength` and `BreathLowPassPoles` are sweepable as strict
  integers;
- `FitnessAbsoluteMin` is recomputed from its defining fields and rejects a direct override;
- `AcuteFatigueMax` and `LongTermFatigueMax` remain independently named coefficients. Their expressions
  define defaults but do not create implicit coupling when `FitnessMaxMets` is overridden;
- asset landmarks, `SecondsPerHour`, `VoiceOutputGain`, and dormant controls are not sweepable and return
  a reason specific to their asset, utility, integration, or unused role.

Validation protects computational domains without imposing narrow calibration ranges: values are finite,
time constants and required denominators are positive, knots and min/max pairs are ordered, normalized
fractions remain in range where the formula requires it, and integer structural controls are valid.
Checks requiring runtime context, such as a high-pass corner below the decoded source's Nyquist frequency,
remain at the consuming boundary.

The propagation audit also found semantic risk calibration outside `Constants.hpp`: the death-risk ramp,
the adrenaline run-risk scale, and the reuse of `VeryHighHeartRateThreshold` as the extreme-HR risk knot.
WI-033 brings those values into the typed rhythm/risk defaults so the selected model value does not leave
hidden calibration inputs in `Runtime.cpp`. Numerical conversions and implementation safety floors remain
fixed unless a separate model decision promotes them.

## Scope and non-goals

Design and implement the immutable coefficient value, default construction, validation, core propagation,
binding surface, and thin CLI mapping needed by WI-028. Keep `Constants.hpp` as the auditable default
parameter index unless the accepted design identifies a single-source alternative that still works with
the existing constant and citation gates.

Do not expose model coefficients as user-facing game configuration, support live mutation of an existing
runtime/source, retune any value, or turn asset landmarks and unit conversions into casual sweep knobs.

A future MCM remains on the Skyrim side of this boundary. It may edit `RuntimeSettings` and adapter-owned
configuration, but it does not expose `ModelCoefficients`, serialize coefficients into a save, or mutate a
coefficient value held by a live runtime. WI-033 does not add live game reconfiguration merely to prepare
for that UI; [WI-034](WI-034-mcm-capability.md) owns the integration.

## Dependencies

None. WI-028 depends on this item because `sim_offline.py`, `rhythm_offline.py`, and the legacy audition
tools currently preserve `--set`.

## Next action and decision points

Implement the typed groups and immutable aggregate, replace behavioral `Constants::` reads with scoped
dependencies, and retain default-delegating overloads for plugin and focused-test callers. Then add the
binding registry, immutable batch override operation, value reporting, validation tests, one focused
override test per group, and default-output invariance coverage before migrating the three thin CLI
clients.

## Newly observed work to split out

MCM capability and live game reconfiguration are split to
[WI-034](WI-034-mcm-capability.md).
