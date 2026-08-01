# Simulation Model

How the physiological state evolves over time - the *input* side of the
[SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md) factor-to-feature map. That document owns how state is mapped
to sound; this one owns how the state itself moves. Pairs with
[REFERENCE_ANALYSIS.md](REFERENCE_ANALYSIS.md) for measurements and
[CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md) for the contractility signal in depth. For term
definitions see [GLOSSARY.md](GLOSSARY.md).

All physiological state and its update loop live in `HeartRateSimulation` (`Simulation.hpp`).
`Runtime::Step` advances it once per frame before coordinating rhythm and acoustic mapping. Tuned
production defaults live in `Constants.hpp` and populate the immutable `ModelCoefficients::Simulation`
group carried by each simulation. This document names each constant and explains the model and its
grounding rather than repeating values - the code is the single source of truth for as-built behavior.

## State variables

`Step` reads a `PlayerState` (the player's current activity) plus the real and in-game time elapsed,
and advances each state variable toward its target on its own timescale:

- **Heart rate** - `m_FastHR` + `m_SlowHR` (`PhysiologySnapshot::HeartRate`,
  `FastHeartRate`, `SlowHeartRate`); target `m_TargetHeartRate`.
- **Exertion** - `m_Exertion` (`PhysiologySnapshot::Exertion`), in METs.
- **Fitness** - `m_Fitness` (`PhysiologySnapshot::Fitness`, `EffectiveFitness`), in METs
  (VO2max / 3.5).
- **Fatigue** - `m_AcuteFatigue`, `m_LongTermFatigue` (`PhysiologySnapshot::AcuteFatigue`,
  `LongTermFatigue`).
- **Adrenaline** - `m_Adrenaline` (`PhysiologySnapshot::Adrenaline`).
- **Contractility** - `m_Contractility` (`PhysiologySnapshot::Contractility`,
  `ContractilityExcess`); modelled in depth by
  [CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md).
- **Respiration** - `m_RespRate`, `m_RespDepth`, `m_RespPhase`
  (`PhysiologySnapshot::RespirationRate`, `RespirationDepth`, `RespirationPhase`).

`GetSnapshot` returns the cohesive outward-facing physiology value, including derived quantities such
as total heart rate, effective fitness, and contractility excess. `GetState` returns the complete
resumable core state instead: it stores both heart-rate components, respiratory phase, and the optional
death timer directly, but excludes derived targets and pending input notifications. `Restore` accepts
that complete value without interpreting missing-field sentinels. The SKSE serialization adapter owns
legacy record optionality and translates it into a `SimulationState`, including the historical defaults
for fields absent from older saves.

## Heart rate

Perceived HR is a two-component (bi-exponential) response: a **fast** vagal component (`m_FastHR`,
weight `HRFastFraction`) and a **slow** sympathetic component (`m_SlowHR`), summed in
`UpdateCurrentHeartRate`. Splitting the response is what lets onset, recovery, and the slow tail run
on different timescales - a single exponential cannot. Each component relaxes toward the target with
its own time constant, and onset and recovery differ (the heart speeds up and slows down at different
rates):

- Onset: `FastOnsetTauSedentary` / `FastOnsetTauElite` (fitness-lerped) and `SlowOnsetTau`.
- Recovery: `FastRecoveryTauSedentary` / `FastRecoveryTauElite` (fitness-lerped) and `SlowRecoveryTau`.

The taus are fitness-dependent (fitter hearts respond faster), lerped between the sedentary and elite
ends by fitness. The target (`ComputeTargetHeartRate`) rises with exertion from a resting floor to a
fitness-scaled ceiling.

The current coefficient grounding is literature-informed but provisional rather than a completed
parameter-level evidence audit. `SleepFraction` follows the common roughly 15% sleeping-HR reduction;
`HRFormulaCeiling` is the absolute endpoint inherited from the 220-minus-age heuristic; and
`HRFastFraction` plus the onset/recovery taus were selected within broad reported response ranges (about
10-20 s for the fitness-dependent fast onset, 30-60 s for fast recovery, 45-90 s for slow onset, and
3-10 min for the slow recovery tail). These ranges justify the scale and ordering, not the exact
setpoints. Revalidation belongs to [WI-012](https://github.com/GrazedAnkle/SHR-SKSE64/issues/15), with provenance
closure under [WI-015](https://github.com/GrazedAnkle/SHR-SKSE64/issues/17).

## Exertion

`m_Exertion` (`UpdateExertion`) accumulates the metabolic cost of the current activity in METs and
clears when the player rests. Activity intensities come from the Compendium of Physical Activities
(`IdleMets`, `WalkingMets`, `RunningMets`, `SprintingMets`, `SwimmingMets`, with crouch and mount
multipliers); impulse activities (`JumpMets`, via `NotifyJump`) are modelled as one-shot events. It
ramps in at `ExertionAccumulationRate` and clears at the slower `ExertionRecoveryRate` (breathing hard
outlasts the effort). Exertion is the metabolic / ventilatory driver: it feeds the HR target,
contractility (as the sympathetic proxy), respiration, and fatigue.

## Fitness

`m_Fitness` (`UpdateFitness`) is the player's aerobic capacity in METs (VO2max / 3.5), adapting slowly
to activity: it rises with sustained exertion (`FitnessGainTau`, on the order of weeks) and decays with
inactivity (`FitnessDecayTau`), bounded by `FitnessBaseMets` .. `FitnessMaxMets`. Fitness sets resting
HR (`RestingHRSlope`, capped at `MaxRestingHR`) and both the HR ceiling and the response taus.
`PhysiologySnapshot::EffectiveFitness` is fitness minus current fatigue (below), floored at
`FitnessAbsoluteMin`.

The present adaptation bracket is weak/provisional grounding carried forward from general training
guidance: `FitnessGainTau` represents roughly eight weeks of aerobic adaptation, `FitnessDecayTau`
roughly four weeks of detraining, and the capacity endpoints correspond to about 24.5 mL/kg/min VO2max
at the deconditioned floor and 70 mL/kg/min at the endurance-athlete ceiling. `RestingHRSlope` uses a
rough 3 bpm/MET sedentary-to-elite relation, while `MaxRestingHR` bounds the supported healthy,
deconditioned end. These figures define a plausible gameplay population; they are not a validated
longitudinal training model.

## Fatigue

Two timescales, both measured in METs and subtracted from fitness (a tired body behaves as if less fit):

- **Acute** - `m_AcuteFatigue` (`UpdateAcuteFatigue`): builds over a workout (`AcuteFatigueGainTau`)
  up to `AcuteFatigueMax`, clears over about an hour (`AcuteFatigueDecayTau`).
- **Long-term** - `m_LongTermFatigue` (`UpdateLongTermFatigue`): accumulates over days of sustained
  load (`LongTermFatigueGainTau`, up to `LongTermFatigueMax`), clears over about a week
  (`LongTermFatigueDecayTau`); sleep accelerates recovery (`SleepRecoveryRate`).

These fatigue setpoints are likewise provisional brackets rather than subject-specific fits. Acute
fatigue uses a 20-30% capacity-loss bracket, a roughly 20-minute build, and about an hour of recovery.
Long-term fatigue uses the upper end of a 5-15% capacity-loss bracket, multi-day accumulation, and a
rough one-to-two-week waking recovery scale; `SleepRecoveryRate` encodes the working assumption that an
eight-hour sleep clears about 55% of the state. WI-012 must audit the coupled trajectories rather than
retuning one fatigue constant in isolation.

## Adrenaline

`m_Adrenaline` (`PhysiologySnapshot::Adrenaline`) is an emotional / acute arousal term with exponential clearance
(`AdrenalineHalfLife`), raised by gameplay events - combat entry (`AdrenalineCombatEntry`,
`NotifyCombatEntry`) and taking a hit (`AdrenalineTakeHit`, `NotifyHit`). The current v1 routing adds it to
the exertion target, so it raises HR through `m_Exertion`; `ContractilityTarget` then reads that exertion
and adds a direct adrenergic contribution through `AdrenalineContractilityScale`. This known double route
is retained until contractility v2 replaces it with separated drivers, as described in
[CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md#deferred-driver-separation).

The roughly two-minute `AdrenalineHalfLife` was taken from the circulating-epinephrine scale reported by
Clutter et al. (1980). That supports the order of magnitude, not the current lumped gameplay state's exact
kinetics or its double routing.

## Respiration

Ventilation is partitioned into two independently lagged states driven by normalized metabolic demand,
not instantaneous HR. `m_RespDepth` is the normalized rest-to-peak tidal-volume excursion (how much air
moves per breath); `m_RespRate` is respiratory frequency. Their steady-state targets are separate
piecewise-linear curves (`ComputeTargetRespDepth`, `ComputeTargetRespRate`): depth rises quickly and is
mostly saturated by VT1 (first ventilatory threshold - where ventilation begins rising faster than
oxygen consumption), while rate rises modestly until RCP (respiratory compensation point - the second
threshold during heavy exercise) and then accelerates to `MaxRespRate`. The knots are owned by
`VentilationVT1Fraction` / `VentilationRCPFraction`, `RespDepthAtVT1` / `RespDepthAtRCP`, and
`RespRateAtVT1` / `RespRateAtRCP`.

The demand normalization divides by effective fitness, so fitness already moves these curves in absolute
workload: at the same MET demand, a fitter character breathes less. The knot fractions and the
rate-versus-depth values are nevertheless shared across fitness levels. Auditing that remaining relative
shape assumption belongs to [WI-012](https://github.com/GrazedAnkle/SHR-SKSE64/issues/15).

This partition follows young-adult exercise data: VT1 occurs around three-quarters of peak oxygen
consumption, tidal volume supplies most of the earlier increase, and respiratory frequency becomes the
dominant late lever ([HUNT 3](https://pmc.ncbi.nlm.nih.gov/articles/PMC4245230/);
[differential-control study](https://pmc.ncbi.nlm.nih.gov/articles/PMC6215760/)). The true maximum remains
an extrapolation because the project's exercise references are submaximal; ref20's ~24 breaths/min at
high HR corroborates the threshold-region target but is not an HR-to-demand calibration point.

`RestingRespRate`, `SleepRespRate`, and `MaxRespRate` sit within broad healthy-young-adult brackets of
roughly 12-20, 8-10, and 47-50 breaths/min respectively. Individual spread is substantial, especially at
maximum, so these are provisional population operating points. `InspirationFraction` preserves the
resting shorter-inspiration premise but is currently dormant; [WI-009](https://github.com/GrazedAnkle/SHR-SKSE64/issues/13)
owns its state-dependent replacement and must coordinate that curve with WI-012's response kinetics.

Rate uses onset `RespOnsetTau` and recovery `RespRecoveryTau`; depth uses
`BreathDepthOnsetTau` / `BreathDepthRecoveryTau`, so deep breathing can outlast the faster rate response.
`SleepRespRate` applies while asleep. `m_RespPhase` is a free-running breath-cycle oscillator (0..1)
driven at the current rate; SYNTHESIS_MODEL consumes phase and depth for the breath muffle, pitch-dip,
and amplitude swing. Respiration also modulates HR through respiratory sinus arrhythmia
(`RSAAmplitudeRest`), the beat-to-beat speeding and slowing across the breath cycle.

## Rhythm

`Runtime` passes each post-step `PhysiologySnapshot` into `RhythmEngine`, which produces optional discrete
`BeatEvent` values (IBI from HR and RSA, plus PVC coupling, compensatory pause, runs, filling timing, and
sampled vigor). The beat *timing* logic lives there; `CreateRenderSpec` combines each fired event with the
same snapshot to produce the per-beat *morphology* (PVC soft S1, systole shortening, and transmission
controls) owned by [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md).

The current PVC probability and coupling endpoints are provisional rhythm priors, not a validated disease
model. `PVCCouplingMax` sits in the reported benign 70-90% IBI range; `PVCCouplingMin` stays just above the
sub-60% R-on-T danger region and matches the project's benign reference PVCs near 0.62. The probability
span runs from a nominal healthy background rate (about 0.026% of beats at 70 bpm) to a deliberately
pathological upper endpoint (about 2.6%). `PVCRunExtensionChance` remains disabled until
[WI-010](https://github.com/GrazedAnkle/SHR-SKSE64/issues/14); [WI-019](https://github.com/GrazedAnkle/SHR-SKSE64/issues/21) owns the
known compensatory-pause scheduling defect.

## Events and time-skip

Gameplay notifications enter through `Runtime` and feed the sim asynchronously: `NotifyJump` (an exertion
impulse), `NotifyCombatEntry` and `NotifyHit` (adrenaline), and `NotifySleep` / `NotifyFastTravel`
(time-skip). A time-skip advances the slow states (fitness, fatigue, long-term recovery) by the elapsed
in-game hours without simulating every frame. State persists across saves via a co-save record (see
`SkyrimHeartRate.cpp`); adapter translation supplies legacy missing-field defaults, including equilibrium
contractility, and the runtime's `Restore` reconstructs the target HR so the first `Step` after a load is
consistent. Fast travel assumes upright walking with no new adrenaline spikes during the skipped interval.

## Deferred dynamics work

The heart-rate and exertion *dynamics* - recovery kinetics versus fitness, the exertion ramp feel, and
the post-exercise systole hysteresis that needs preload and afterload terms - have a dedicated focused
pass: [WI-012](https://github.com/GrazedAnkle/SHR-SKSE64/issues/15). Its accepted literature-backed model lands in
this document when that work is complete.
