# Contractility Model

Contractility (how forcefully the heart squeezes) is the slow inotropic state shared by several audio
features. This document owns its current signal, kinetics, interface, and consumers. How those consumers
map state to sound is explained in [SYNTHESIS_MODEL.md](SYNTHESIS_MODEL.md); the rest of physiological
state evolution is in [SIMULATION_MODEL.md](SIMULATION_MODEL.md).

## Current scope

`HeartRateSimulation` exposes contractility as audio-only values through
`PhysiologySnapshot::Contractility` and `PhysiologySnapshot::ContractilityExcess`. The scalar itself does
not feed back into heart rate, exertion, or metabolic demand. Its current v1 drivers are not cleanly
separated, however: `UpdateExertion` adds `m_Adrenaline` to its target, routing acute arousal through
exertion and HR, while `ContractilityTarget` reads that exertion and adds `m_Adrenaline` again as a direct
adrenergic term. Contractility v2 owns removal of this known double route rather than treating it as an
isolated coefficient fix.

The public interface is deliberately narrower than the internal driver model. A future separation of
sympathetic nervous drive and circulating catecholamines can replace the internals without changing the
audio consumers.

## Target and kinetics

`UpdateContractility` computes normalized metabolic demand and an adrenergic contribution:

```text
normalizedExertion = clamp((m_Exertion - IdleMets) / (effectiveFitness - IdleMets), 0, 1)
adrenergic         = clamp(m_Adrenaline / AdrenalineContractilityScale, 0, 1)
target             = clamp(normalizedExertion + adrenergic, 0, 1)
```

In v1, `m_Exertion` already contains the lagged effect of `m_Adrenaline`, so the two terms above are not
independent. The equations describe the as-built signal, not the intended v2 driver boundary.

`m_Contractility` relaxes toward that target with asymmetric exponential kinetics:

```text
tau = target > m_Contractility ? ContractilityOnsetTau : ContractilityDecayTau
m_Contractility += (1 - exp(-realDelta / tau)) * (target - m_Contractility)
```

`ContractilityOnsetTau` represents sympathetic/inotropic activation. `ContractilityDecayTau` represents
the slower clearance of circulating catecholamines and is the main recovery-hysteresis timescale. The
references establish the lingering direction but do not pin the exact decay; literature and ear set the
current operating point.

## Recovery hysteresis

When exertion stops, the target drops with `ExertionRecoveryRate`, while `m_Contractility` decays more
slowly. The fast vagal heart-rate component can therefore recover while S1 remains forceful and the
contractility-dependent systole correction remains active. This produces the post-exercise separation
between heart rate and beat character observed in refs 7, 8, and 12.

The current scalar cannot reproduce all of the reference systole shape by itself. Filling/preload and
post-exercise afterload changes have distinct recovery signatures and are absent from the current systole
correction. They must be rebalanced together with the sympathetic term; see
[WI-012](https://github.com/GrazedAnkle/SHR-SKSE64/issues/15).

## Audio consumers

`RhythmEngine` samples per-beat `BeatEvent::Vigor` from the mean contractility. The acoustic map then
combines that event fact with the firing `PhysiologySnapshot`; respiratory depth and phase remain separate
transmission inputs, while `RhythmInput::ExertionFraction` only attenuates RSA with exertion.

1. **S1 loudness.** `CreateRenderSpec` combines event vigor with the per-beat Frank-Starling filling term.
   `ContractilityGainDb` controls the rest-to-forceful span; unity at rest preserves the source's resting
   level.
2. **Systole recovery correction.** The steady-state line already includes the drive associated with its
   HR. In the acoustic map, `SystolePEPShortening` therefore multiplies
   `PhysiologySnapshot::ContractilityExcess`, the contractility above what current HR implies, rather than
   total contractility.
3. **S1 onset and brightness.** `CreateRenderSpec` derives onset compression from the same event vigor
   and filling term; `RenderBeat` applies that scalar to the cached baseline S1 attack region. One
   physiological driver therefore makes a forceful beat louder, faster-rising, and brighter while
   preserving the source's post-peak body.
4. **Beat-to-beat variation.** `RhythmEngine` applies bounded vigor jitter around the mean contractility.
   The same per-beat draw moves loudness and envelope character together.

Contractility does **not** drive audible saturation. `SoftClipKnee` is a level-driven output limiter that
protects the loudest jittered peaks; capture-chain grit is not part of the default heart model. PVC
dulling is also separate and uses `ResamplePVCRatio`.

## Interface and persistence

- `HeartRateSimulation` owns `m_Contractility` and updates it from exertion and adrenaline.
- `Runtime::Step` passes `PhysiologySnapshot::Contractility` into `RhythmEngine::Advance`, then combines
  the resulting event with the complete snapshot through `CreateRenderSpec`.
- `RhythmEngine` stores sampled per-beat force in `BeatEvent::Vigor`; `CreateRenderSpec` converts it and
  `ContractilityExcess` into controls consumed by the core beat renderer; `HeartbeatVoice::Play` forwards
  the resolved specification to that renderer.
- The CTLY co-save record persists the state. Saves without that field seed contractility to the restored
  exertion/adrenaline equilibrium.

As-built values and provenance tags live in `src/core/Constants.hpp`. The focused provenance audit is
[WI-015](https://github.com/GrazedAnkle/SHR-SKSE64/issues/17).

## Deferred driver separation

Contractility v2 will split the current slow state into first-class sympathetic/noradrenergic and
circulating epinephrine drivers while retaining the existing snapshot boundary. In particular, it will
replace the v1 adrenaline-through-exertion plus direct-adrenergic double route rather than tuning around it:

- HR becomes a consequence of vagal withdrawal plus the separated sympathetic/catecholamine terms.
- Contractility weights those drivers plus Frank-Starling filling.
- An epinephrine getter can support fight-or-flight gameplay without routing gameplay effects through the
  audio scalar.
- Respiratory rate/depth can gain an arousal input so low-exertion fear affects both the fast beat jitter
  and slower breathing cues.

That change requires re-deriving HR kinetics and is intentionally later than the current evidence-safe
audio work. It is a model replacement, not an unfinished part of the current signal.
