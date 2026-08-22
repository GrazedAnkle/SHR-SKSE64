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

## Clocks

The model advances on two clocks, and the disagreement between them is a deliberate liberty rather than an
oversight. `Step` takes both intervals - a real frame delta and elapsed in-game hours - and every state
variable is credited on exactly one of them.

**The acute layer integrates experienced time**: heart rate, exertion, adrenaline, acute fatigue,
contractility, and respiration advance over the interval the character lives through, which is one second
per real second while the game runs and the whole skipped span across a time-skip. A character who sleeps
eight hours must wake with adrenaline cleared; the real time the menu occupied is not what their body
experienced.

**The chronic layer integrates the game calendar**: fitness and long-term fatigue advance on
`StepInput::GameHoursDelta` unconditionally. Their only observable is drift across in-game days, and the
calendar is what a time-skip advances anyway, so they need no skip handling of their own.

Neither layer can move to the other's clock. The acute layer's output is a beat interval, heard in real
seconds and calibrated against real-time recordings, so slowing it by the timescale would invalidate every
reference comparison in [REFERENCE_ANALYSIS.md](REFERENCE_ANALYSIS.md). The chronic layer's `FitnessGainTau`
is eight weeks, which a hundred-hour playthrough would not traverse a tenth of, so on experienced time the
fitness model would never move at all.

### What crosses the seam

The two couplings are not symmetric. Downward, the chronic layer exposes `EffectiveFitness`, a slowly
varying scalar the acute layer reads as a parameter; no interval travels with it, so that direction is
clock-free. Upward, two chronic integrators take acute state as their driver: `UpdateLongTermFatigue` reads
normalized `m_AcuteFatigue`, and `UpdateFitness` gates on `m_Exertion` and scales by a training efficacy
derived from both fatigue states. Those two expressions are where the clocks meet.

### The bound

One physiological event therefore has two durations, differing by the timescale - 20 by default, and
user-configurable. Recovery from a hard fight clears acute fatigue over `AcuteFatigueDecayTau`, one real
hour, while the calendar advances twenty hours across that same interval and `UpdateLongTermFatigue` is
told the character stayed exhausted for all of them. Against a `LongTermFatigueDecayTau` of one week the
driver's own transient is an eighth of the integrator's time constant; `AcuteFatigueGainTau` maps to nearly
seven game hours against a three-day `LongTermFatigueGainTau`.

The chronic integrators therefore cannot treat their drivers as quasi-static, which is the property the
words *acute* and *long-term* are meant to promise. That is the whole of it: the ratio is fixed at the
timescale rather than accumulating, the exponential steps are exact so it does not depend on frame rate,
and the downward coupling does not carry it back. The two durations are irreducible because neither layer
can move, and each layer is right about the thing it is observed through - the beat is heard in real time,
and progression is read off the calendar.

Two consequences follow. Progression pace measured in real play hours is a chronic time constant divided by
the timescale, so it is set partly by a game setting this model does not own. And the training stimulus
reaching `UpdateFitness` runs at whatever fraction of the calendar the player spends moving, which for an
adventurer is most of it, where the published adaptation timescales behind `FitnessGainTau` were measured
on programs occupying a few percent of a week. That duty-cycle gap belongs to the driver rather than to the
clock; [#28](https://github.com/GrazedAnkle/SHR-SKSE64/issues/28) brackets those rates and must account
for it.

## Heart rate

Perceived HR is a two-component (bi-exponential) response: a **fast** vagal component (`m_FastHR`,
weight `HRFastFraction`) and a **slow** sympathetic component (`m_SlowHR`), summed in
`UpdateCurrentHeartRate`. Splitting the response is what lets onset, recovery, and the slow tail run
on different timescales - a single exponential cannot. Each component relaxes toward the target with
its own time constant, and onset and recovery differ (the heart speeds up and slows down at different
rates):

- Onset: `FastOnsetTauSedentary` / `FastOnsetTauElite` (fitness-lerped) and `SlowOnsetTau`.
- Recovery: `FastRecoveryTauSedentary` / `FastRecoveryTauElite` (fitness-lerped) and `SlowRecoveryTau`.
  The lerp normalizes against the global `FitnessEliteMets`, not the character's own ceiling: the
  endpoints name absolute states, so a low-capacity character must not reach the elite tau.

The taus are fitness-dependent (fitter hearts respond faster), lerped between the sedentary and elite
ends by fitness. The target (`ComputeTargetHeartRate`) rises with exertion from a resting floor to a
fitness-scaled ceiling.

[LITERATURE_ANALYSIS.md](LITERATURE_ANALYSIS.md#heart-rate-response-kinetics) owns the published brackets
for these coefficients and for the resting line beneath them. Three of its conclusions bear on the
structure above rather than only on the values. No published time constant brackets either component
alone, because the literature fits the whole response, so only the composite of the two is comparable;
the onset pair matches its bracket at both ends while composite recovery is slower than every intensity
measured; and `HRFastFraction`, which sets the split itself, has no published bracket and is a
consequence of whichever recovery taus are chosen rather than an independent setting.

Revalidation belongs to [#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27), which re-derives
these taus from the separated drivers, with provenance closure under
[WI-015](https://github.com/GrazedAnkle/SHR-SKSE64/issues/17).

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
inactivity (`FitnessDecayTau`), bounded below by `FitnessBaseMets` and above by the per-character
`SimulationSettings::FitnessMaxMets`. Fitness sets resting
HR (`RestingHRSlope`, capped at `MaxRestingHR`) and both the HR ceiling and the response taus.
`PhysiologySnapshot::EffectiveFitness` is fitness minus current fatigue (below), floored at
`FitnessAbsoluteMin`.

Fitness is also what makes `RestingHeartRate` a seed rather than a parameter: `CreateInitialState` is its
only consumer, so a control bound straight to it would change nothing on a character who has already
played. Because that seeding is linear, shifting live fitness by the same slope is the one transform that
commutes with drift - re-seeding would delete the character's progression, and doing nothing would
misrepresent what the control does. `HeartRateSimulation::ApplySettings` performs the shift and clamps only
from below, so lowering the ceiling lets a character detrain over `FitnessDecayTau` rather than losing
progression at the moment the control moves.

The capacity endpoints `FitnessBaseMets` and `FitnessEliteMets` are bracketed against population and
athlete reference values in
[LITERATURE_ANALYSIS.md](LITERATURE_ANALYSIS.md#aerobic-capacity-endpoints), which also records why
`FitnessAbsoluteMin` is a division guard rather than a physiological floor. The adaptation rates are
weak/provisional grounding from general training guidance: `FitnessGainTau` represents
roughly eight weeks of aerobic adaptation and `FitnessDecayTau` roughly four weeks of detraining, while
`RestingHRSlope` uses a rough 3 bpm/MET sedentary-to-elite relation and `MaxRestingHR` bounds the
supported healthy, deconditioned end. Those four define a plausible gameplay population; they are not a
validated longitudinal training model.

## Fatigue

Two timescales, both measured in METs and subtracted from fitness (a tired body behaves as if less fit):

- **Acute** - `m_AcuteFatigue` (`UpdateAcuteFatigue`): builds over a workout (`AcuteFatigueGainTau`)
  up to `AcuteFatigueMaxFraction` of raw fitness, clears over about an hour (`AcuteFatigueDecayTau`).
- **Long-term** - `m_LongTermFatigue` (`UpdateLongTermFatigue`): accumulates over days of sustained
  load (`LongTermFatigueGainTau`, up to `LongTermFatigueMaxFraction` of raw fitness), clears over about a week
  (`LongTermFatigueDecayTau`); sleep accelerates recovery (`SleepRecoveryRate`).

The capacity-loss magnitudes `AcuteFatigueMaxFraction` and `LongTermFatigueMaxFraction` are bracketed against the
durability literature in
[LITERATURE_ANALYSIS.md](LITERATURE_ANALYSIS.md#fatigue-reduction-of-aerobic-capacity), which also
records why the reduction is proportional to capacity rather than absolute, and why that does not
conflict with dosing exertion in absolute METs. The gain and decay rates are provisional rather than
bracketed: acute fatigue uses a roughly 20-minute build and about an hour of recovery, long-term
fatigue multi-day accumulation and a rough one-to-two-week waking recovery scale, and
`SleepRecoveryRate` encodes the working assumption that an eight-hour sleep clears about 55% of the
state. [#28](https://github.com/GrazedAnkle/SHR-SKSE64/issues/28) brackets those rates and must audit the
coupled trajectories rather than retuning one fatigue constant in isolation. It also inherits both
consequences recorded in [Clocks](#clocks): `UpdateLongTermFatigue` takes its driver from the other side
of the clock seam, and the stimulus reaching these integrators runs at a duty cycle no training program
resembles, so neither rate can be read straight off a published calendar duration. The saturation policy that
makes `UpdateAcuteFatigue` clamp normalized exertion where `NormalizedExertion` does not belongs to
[#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27), which has to choose a saturation convention
for the driver model anyway.

## Adrenaline

`m_Adrenaline` (`PhysiologySnapshot::Adrenaline`) is an emotional / acute arousal term with exponential clearance
(`AdrenalineHalfLife`), raised by gameplay events - combat entry (`AdrenalineCombatEntry`,
`NotifyCombatEntry`) and taking a hit (`AdrenalineTakeHit`, `NotifyHit`). The current v1 routing adds it to
the exertion target, so it raises HR through `m_Exertion`; `ContractilityTarget` then reads that exertion
and adds a direct adrenergic contribution through `AdrenalineContractilityScale`. Contractility v2 replaces
this known double route with separated drivers, as described in
[CONTRACTILITY_SPEC.md](CONTRACTILITY_SPEC.md#driver-separation).

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
(time-skip), which covers sleeping, waiting, and fast travel. A time-skip notification advances the
*acute* states over the skipped interval through `AdvanceTimeSkip`, which is what [Clocks](#clocks) means
by experienced time; sleep takes its own path instead, setting the sleeping heart rate and respiration
directly and clearing long-term fatigue at `SleepRecoveryRate`. The chronic states need no notification,
because the skipped hours reach them as an ordinary large `StepInput::GameHoursDelta` on the next live
frame; advancing them in the skip as well would credit it twice.

Each skip measures its interval from the game clock's held reading rather than from its event, so all three
agree on what was skipped and an interrupted wait credits what the calendar actually advanced rather than
what the player asked for. Fast travel assumes upright walking with no new adrenaline spikes across the
interval; waiting settles to the stationary baseline instead. State persists across saves via a co-save
record (see
`SkyrimHeartRate.cpp`); adapter translation supplies legacy missing-field defaults, including equilibrium
contractility, and the runtime's `Restore` reconstructs the target HR so the first `Step` after a load is
consistent.

## Dynamics work in progress

The heart-rate and exertion *dynamics* are split across two passes, and each lands its accepted
literature-backed model in this document when complete.

Recovery kinetics versus fitness and the exertion ramp belong to
[#27](https://github.com/GrazedAnkle/SHR-SKSE64/issues/27), which replaces the driver model they hang
off rather than retuning them in place. The post-exercise systole hysteresis that needs preload and
afterload terms belongs to [WI-012](https://github.com/GrazedAnkle/SHR-SKSE64/issues/15), and waits on
#27 for the sympathetic term it weights.
