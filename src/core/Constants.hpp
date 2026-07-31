/*
 * This file is part of SHR.
 *
 * SHR is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, version 3.
 *
 * SHR is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * SHR. If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <cstdint>

namespace SHR::Constants
{
    // Compact parameter index. The model docs own rationale; this file keeps semantics,
    // units, domains, sentinels, implementation coupling, and provenance. Tag definitions:
    // docs/DOCUMENTATION_CONVENTIONS.md.

    // ============================ SIMULATION MODEL ===========================

    // --- Heart-rate dynamics [physio] ---
    constexpr float BaseRestingHR            = 79.0F;        // bpm; rest at FitnessBaseMets
    constexpr float SleepFraction            = 1.0F - 0.15F; // [0, 1]; multiplier on effective resting HR
    constexpr float HRFormulaCeiling         = 220.0F;       // bpm; absolute target-HR cap
    constexpr float HRFastFraction           = 0.6F;         // [0, 1]; fast-component weight
    constexpr float FastOnsetTauSedentary    = 20.0F;        // s; fast-component onset tau at low fitness
    constexpr float FastOnsetTauElite        = 10.0F;        // s; fast-component onset tau at high fitness
    constexpr float SlowOnsetTau             = 60.0F;        // s; slow-component onset tau
    constexpr float FastRecoveryTauSedentary = 60.0F;        // s; fast-component recovery tau at low fitness
    constexpr float FastRecoveryTauElite     = 30.0F;        // s; fast-component recovery tau at high fitness
    constexpr float SlowRecoveryTau          = 3.0F * 60.0F; // s; slow-component recovery tau

    // --- Activity intensity (METs) [physio] ---
    constexpr float IdleMets      =  1.5F; // MET; stationary baseline
    constexpr float WalkingMets   =  3.0F; // MET; walking
    constexpr float RunningMets   =  7.0F; // MET; running
    constexpr float SprintingMets = 15.0F; // MET; sprinting
    constexpr float SwimmingMets  =  6.0F; // MET; swimming replaces active land movement
    constexpr float JumpMets      =  3.0F; // [game] MET impulse per jump

    // --- Movement & exertion modifiers [game] ---
    constexpr float CrouchMovementMultiplier = 1.3F; // x active movement contribution
    constexpr float MountedMultiplier        = 0.6F; // x active movement contribution
    constexpr float ExertionAccumulationRate = 4.0F; // MET/s; maximum upward slew
    constexpr float ExertionRecoveryRate     = 1.0F; // MET/s; maximum downward slew

    // --- Adrenaline [physio] ---
    constexpr float AdrenalineHalfLife    = 2.0F * 60.0F; // s; exponential clearance half-life
    constexpr float AdrenalineCombatEntry = 3.0F;         // [game] additive event impulse
    constexpr float AdrenalineTakeHit     = 2.0F;         // [game] additive event impulse

    // --- Contractility (inotropy) drive [physio] ---
    constexpr float ContractilityOnsetTau        = 20.0F;  // s; normalized-drive onset tau
    constexpr float ContractilityDecayTau        = 120.0F; // s; normalized-drive decay tau
    constexpr float AdrenalineContractilityScale = 3.0F;   // [dsp] adrenaline units producing adrenergic = 1

    // --- Long-term fitness [physio] ---
    constexpr float FitnessGainTau  = 8.0F * 7.0F * 24.0F; // game h; training adaptation tau
    constexpr float FitnessDecayTau = 4.0F * 7.0F * 24.0F; // game h; detraining tau
    constexpr float FitnessBaseMets = 24.5F / 3.5F;         // MET; detraining floor
    constexpr float FitnessMaxMets  = 70.0F / 3.5F;         // MET; adaptation ceiling
    constexpr float RestingHRSlope  = 3.0F;                 // bpm/MET; fitness-to-resting-HR slope
    constexpr float MaxRestingHR    = 90.0F;                // bpm; supported deconditioned ceiling
    // Absolute fitness floor derived from MaxRestingHR. Prevents division by
    // zero in ComputeTargetHeartRate, and caps the effect of fatigue on EffectiveFitness.
    constexpr float FitnessAbsoluteMin   = FitnessBaseMets + (BaseRestingHR - MaxRestingHR) / RestingHRSlope;

    // --- Respiration [physio] ---
    constexpr float RestingRespRate        = 14.0F; // breaths/min; awake resting target
    constexpr float VentilationVT1Fraction =  0.75F; // [0, 1]; normalized aerobic-reserve knot
    constexpr float VentilationRCPFraction =  0.86F; // [0, 1]; normalized aerobic-reserve knot
    constexpr float RespRateAtVT1          = 26.0F;  // breaths/min; rate target at VT1
    constexpr float RespRateAtRCP          = 31.0F;  // breaths/min; rate target at RCP
    constexpr float RespDepthAtVT1         =  0.85F; // [0, 1]; rest-to-peak depth target at VT1
    constexpr float RespDepthAtRCP         =  0.93F; // [0, 1]; rest-to-peak depth target at RCP
    constexpr float MaxRespRate            = 50.0F; // breaths/min; maximal-exertion target
    constexpr float SleepRespRate          =  9.0F; // breaths/min; sleep target
    // First-order placeholder kinetics; WI-012 owns replacement with a phase-structured response.
    constexpr float RespOnsetTau           = 15.0F; // s; respiratory-rate onset tau
    constexpr float RespRecoveryTau        = 30.0F; // s; respiratory-rate recovery tau
    constexpr float BreathDepthOnsetTau    = 15.0F; // s; tidal-depth onset tau, ear-pinned to RespOnsetTau
    constexpr float BreathDepthRecoveryTau = 60.0F; // s; tidal-depth recovery tau, ear-set
    // Dormant [0, 1] cycle fraction; WI-009 owns state-dependent timing, coordinated with WI-012.
    constexpr float InspirationFraction    =  0.4F;

    // --- Respiratory sinus arrhythmia [physio] ---
    constexpr float RSAAmplitudeRest = 0.05F; // fraction; resting +/- IBI modulation, fades with exertion

    // --- Premature ventricular contractions: rhythm [physio] ---
    constexpr float PVCCouplingMax       = 0.85F; // normal-IBI fraction; low-risk endpoint
    constexpr float PVCCouplingMin       = 0.62F; // normal-IBI fraction; peak-risk endpoint
    constexpr float PVCCouplingVariation = 0.04F; // +/- multiplicative coupling jitter
    // Intended pause = (2 - coupling) * IBI; WI-019 owns the current scheduling defect.
    constexpr float PVCPauseVariation     = 0.05F; // +/- multiplicative pause jitter
    constexpr float PVCChanceNormal       = 0.0003F; // probability/s; baseline susceptibility endpoint
    constexpr float PVCChanceMax          = 0.03F;   // probability/s; maximum susceptibility endpoint
    // [0, 1] per-additional-beat probability at unit susceptibility; 0 = disabled pending WI-010.
    constexpr float PVCRunExtensionChance = 0.0F;
    constexpr int   PVCRunMaxLength       = 5; // beats; maximum geometric-run length
    // --- Arrhythmia risk calibration ---
    constexpr float DeathRiskRampSeconds          = 4.0F * 60.0F; // [game] s; dying-state ramp to maximum risk
    constexpr float ExtremeHeartRateRiskThreshold = 170.0F;       // [game] bpm; lower knot of extreme-HR risk; TODO: can this be anchored to physio?
    constexpr float AdrenalineRunRiskScale        = 5.0F;         // [game] adrenaline units producing run-risk = 1

    // --- Acute fatigue [physio] ---
    constexpr float AcuteFatigueMax      = 0.25F * FitnessMaxMets; // MET; subtracted from fitness
    constexpr float AcuteFatigueGainTau  = 20.0F * 60.0F;         // s; accumulation tau
    constexpr float AcuteFatigueDecayTau = 60.0F * 60.0F;         // s; recovery tau

    // --- Long-term fatigue [physio] ---
    constexpr float LongTermFatigueMax      = 0.15F * FitnessMaxMets; // MET; subtracted from fitness
    constexpr float LongTermFatigueGainTau  = 3.0F * 24.0F;          // game h; accumulation tau
    constexpr float LongTermFatigueDecayTau = 7.0F * 24.0F;          // game h; waking recovery tau
    constexpr float SleepRecoveryRate       = 0.099F; // h^-1; exp(-rate * sleep hours)

    // --- Unit conversions [util] ---
    constexpr float SecondsPerHour = 60.0F * 60.0F;

    // ============================ AUDIO SYNTHESIS ============================

    // --- Heartbeat sample landmarks [asset] ---
    constexpr std::uint32_t S1OnsetFrames = 2400;  // frame @48 kHz; 50 ms
    constexpr std::uint32_t S1EndFrames   = 6960;  // frame @48 kHz; 145 ms, before editing bump
    constexpr std::uint32_t S2OnsetFrames = 17760; // frame @48 kHz; 370 ms
    constexpr std::uint32_t S2EndFrames   = 23856; // frame @48 kHz; 497 ms
    constexpr float         CrossfadeMs   = 5.0F;  // [dsp] ms; linear boundary taper

    // --- Source low-cut (recording-chain conditioning) [dsp] ---
    constexpr float SourceHighPassHz = 40.0F; // Hz; 2-pole Butterworth before NormalizeJoint, 0 = disabled

    // --- High-rate sound shortening [dsp] ---
    constexpr float S1SystoleFraction = 0.70F; // [0, 1]; S1 cap as fraction of systole
    constexpr float S2WindowFraction  = 0.50F; // [0, 1]; S2 cap as fraction of remaining IBI

    // --- Onset build-compression [dsp] ---
    constexpr float AttackCompressMax = 2.5F; // >=1; maximum final-ascent speedup; reached with vigor jitter
    // Last pre-peak crossing on the analytic envelope, shared with shrlib.onset_peak_idx. The threshold
    // must remain between the source's ~2.35% null and ~55% precursor lobe or it selects another ascent.
    constexpr float AttackBuildThreshold = 0.10F; // onset = last pre-peak crossing of this fraction of the S1 peak

    // --- PVC dulling [dsp] ---
    constexpr float ResamplePVCRatio = 0.90F; // (0, 1]; ectopic-S1 playback/pitch ratio

    // --- Breath-phase modulation [dsp] ---
    // Shared transmission driver: breathDepthFactor * sin(pi * RespPhase), clamped to [0, 1].
    constexpr float BreathAmpDepth          = 0.47F; // [0, 1]; peak-inflation amplitude attenuation
    constexpr float BreathDepthRestFraction = 0.40F; // [0, 1]; resting depth as fraction of maximum
    constexpr float BreathPitchDipDepth     = 0.14F; // [0, 1]; maximum resample/F0 drop
    constexpr float BreathLowPassOpenHz     = 450.0F; // Hz; expiration cutoff
    constexpr float BreathLowPassMinHz      = 112.0F; // Hz; peak-inspiration cutoff
    constexpr int   BreathLowPassPoles      = 2;      // >=1; cascaded one-pole count

    // --- Systole duration model (S1 onset to S2 onset), in seconds [ref] ---
    //   systole = clamp(SystoleIntercept - HR * SystoleSlope, SystoleMin, SystoleMax)
    constexpr float SystoleIntercept = 0.465F;   // s; fitted audible S1-to-S2 intercept
    constexpr float SystoleSlope     = 0.00171F; // s/bpm; fitted HR slope
    // Audible-interval floor, not an HR or LVET floor; WI-012/WI-015 own its unaudited setpoint.
    constexpr float SystoleMin       = 0.130F; // [physio] s; high-rate clamp
    constexpr float SystoleMax       = 0.400F; // [dsp] s; low-rate runaway clamp
    constexpr float SystolePEPShortening = 0.10F; // s; state-unbindable; WI-012 owns the mechanism audit

    // --- PVC systole [dsp] ---
    constexpr float PVCSystoleScale = 0.55F;  // x nominal sinus systole; intentionally coupling-independent
    constexpr float PVCSystoleMin   = 0.150F; // s; ectopic high-rate floor

    // --- Contractility gain (cardiac source stage) [dsp] ---
    constexpr float ContractilityGainDb = 11.0F; // dB; rest-to-peak S1 loudness span, unity at rest

    // --- Frank-Starling S1 amplitude scaling [dsp] ---
    constexpr float FrankStarlingMin = 0.50F; // amplitude x at zero relative filling; <=1
    constexpr float FrankStarlingMax = 1.50F; // amplitude x at full relative filling; >=1

    // --- Loudness / saturation model [dsp] ---
    // knee + (1-knee)*tanh(over) keeps |buffer| strictly below full scale for any upstream drive.
    constexpr float SourceRestLevel = 0.17F; // [0, 1] full-scale; normalized resting-source peak
    constexpr float SoftClipKnee    = 0.50F; // (0, 1) full-scale; tanh limiter onset

    // --- Beat-to-beat vigor jitter (cardiac dynamics) [ref] ---
    constexpr float VigorJitterScale = 0.17F; // sd; Gaussian contractility jitter x mean contractility
    // One-sided clamp: preserve the Gaussian lower tail and ordinary above-mean beats; bound only the
    // extreme positive draw before it flat-tops into SoftClipKnee.
    constexpr float VigorJitterMaxSigma = 2.5F; // sd; maximum positive draw

    // --- Output gain (engine-mix integration) [game] ---
    constexpr float VoiceOutputGain = 4.0F; // x SetVolume; compensates the game's ~0.25x mix attenuation

    // --- PVC beat audio amplitudes [dsp] ---
    constexpr float PVCS1Amplitude = 0.75F; // base x; also receives Frank-Starling filling
    constexpr float PVCS2Amplitude = 0.60F; // base x; also receives coupling/perfusion ramp
    constexpr float PVCS2FailCoupling = 0.45F; // [physio] IBI fraction; S2 perfusion = 0
    constexpr float PVCS2FullCoupling = 0.65F; // [physio] IBI fraction; S2 perfusion = 1
}
