# WI-012: Bundled Simulation and Dynamics Pass

Status: `[DEFERRED]`

## Outcome and acceptance criteria

Heart-rate, exertion, ventilation, and post-exercise systole dynamics form one coherent response across
fitness levels. The pass has trajectory tests and literature/reference grounding for each changed shape,
then passes an in-game rest-to-exercise-to-recovery audition.

## Current conclusion

The largest systole-hysteresis residual is a missing-mechanisms problem, not a request to slow HR recovery
or lengthen the existing contractility time constant. Lower HR increases filling, and post-exercise
afterload can fall; both can make shortening grow during recovery while contractility itself decays.

Exertion currently accumulates eagerly. Ventilation has improved steady-state targets but first-order
kinetics omit the initial neural step and slower second phase, and maximal recovery ordering may be wrong.
Capacity normalization already makes the same absolute workload produce less ventilation in a fitter
character, but VT1/RCP positions and the rate-versus-depth partition are fixed at the same *relative*
demand for every fitness level. Training literature indicates those relative thresholds can also move.

Systole currently follows smoothed HR plus an unjittered contractility-excess correction. It does not
inherit the per-beat vigor draw. Real beat-level timing can also respond to preceding filling, afterload,
and activation, so unexplained reference scatter is not sufficient evidence for independent white systole
jitter. These interactions should not be tuned as isolated constants.

`SystoleMin` is a hard floor applied after extrapolating the reference-fitted line; the line reaches it at
about 196 bpm, but that crossover is not evidence that audible S1-to-S2 duration clamps there. Published
QS2 work supports inverse, often locally linear HR relations at rest and over about 120-170 bpm during
exercise, while also finding exercise-intensity dependence; QS2 begins at ECG QRS onset and is not the
same interval as audible S1 onset to S2 onset. A smooth cubic transition to an asymptote is therefore a
candidate implementation shape, not a literature-derived law or accepted replacement. Its purpose would
be to preserve value and slope in the measured range without the hard-floor kink while making the
high-rate limit explicit.

## Scope and non-goals

When activated, sequence:

1. remeasure time-stamped recovery evidence and low-fitness HR recovery;
2. add/rebalance preload, afterload, and sympathetic systole terms;
3. audit exertion ramp and steady-state HR versus metabolic demand;
4. compare the linear-plus-floor systole law with bounded smooth-transition candidates, including a cubic,
   while keeping ECG-QS2 and audible S1-to-S2 definitions separate;
5. decide whether residual beat-level systole variation follows causal preload/afterload/inotropy terms
   before considering an independent jitter term;
6. audit fitness-dependent VT1/RCP positions and respiratory-rate/tidal-depth partition without
   duplicating the existing capacity normalization;
7. design phase-structured ventilation onset/recovery; and
8. validate swimming/breath-hold only if suitable reference evidence appears.

Sitting/resting posture and gameplay consequences are separate later work.

## Dependencies

- Contractility/recovery classifications in the
  [reference-state ledger](../MEASUREMENT_METHODS.md#reference-state-ledger).
- Settled S2 timing ruler in [MEASUREMENT_METHODS.md](../MEASUREMENT_METHODS.md#preferred-rulers) when
  interpreting small per-beat systole residuals.
- Reference findings in [REFERENCE_ANALYSIS.md](../REFERENCE_ANALYSIS.md).
- Current state evolution in [SIMULATION_MODEL.md](../SIMULATION_MODEL.md).

## Next action and decision points

When reprioritized, start with the ref12 timestamped recovery pass and a low/medium/high-fitness trajectory
matrix in `tools/sim_offline.py`. Include matched absolute-workload and matched relative-demand ventilation
views so the two fitness effects remain distinguishable. The maintainer decides the
responsiveness-versus-physiological-kinetics tradeoff by auditioning whole trajectories, not isolated
fixtures.

## Observed separate work

Contractility driver separation and fight-or-flight gameplay remain a later architectural milestone.
