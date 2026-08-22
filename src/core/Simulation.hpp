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

#include "core/ModelCoefficients.hpp"
#include "core/PhysiologySnapshot.hpp"
#include "core/SimulationSettings.hpp"
#include "core/SimulationState.hpp"

namespace SHR
{
    struct VentilationTargets
    {
        float Rate;
        float Depth;
    };

    VentilationTargets ComputeVentilationTargets(float normalizedExertion);
    VentilationTargets ComputeVentilationTargets(
        float normalizedExertion,
        const SimulationModelCoefficients &coefficients
    );

    struct PlayerState
    {
        bool IsDead      = false;
        bool IsSprinting = false;
        bool IsRunning   = false;
        bool IsWalking   = false;
        bool IsSwimming  = false;
        bool IsSneaking  = false;
        bool IsOnMount   = false;
    };

    class HeartRateSimulation
    {
    public:
        explicit HeartRateSimulation(SimulationSettings settings)
            : HeartRateSimulation(settings, DefaultModelCoefficients().Simulation)
        {
        }

        HeartRateSimulation(
            SimulationSettings          settings,
            SimulationModelCoefficients coefficients
        )
            : m_Settings(settings)
            , m_Coefficients(coefficients)
        {
        }

        void Init();
        // realDelta is frame time in seconds; gameHoursDelta is elapsed in-game hours.
        void Step(PlayerState state, float realDelta, float gameHoursDelta = 0.0F);

        // Live replacement: shifts fitness with the resting rate. Runtime validates first; this
        // applies unconditionally.
        void ApplySettings(SimulationSettings settings);

        // Update thread only: reached solely from Runtime's mailbox drain.
        void NotifyJump();
        // Durations are experienced seconds: the skipped in-game span, not the real time it took.
        void NotifySleep(float duration);
        void NotifyFastTravel(float duration);
        void NotifyWait(float duration);
        void NotifyCombatEntry();
        void NotifyHit();

        PhysiologySnapshot GetSnapshot() const;
        float GetTargetHeartRate() const noexcept;
        float GetTargetRespirationRate() const;
        float GetTargetRespirationDepth() const;
        SimulationState GetState() const;
        SimulationState CreateInitialState() const;
        float ComputeEquilibriumContractility(const SimulationState &state) const;
        void Restore(const SimulationState &state);

    private:
        static constexpr float Sentinel = -1.0F;

        float ComputeTargetRespRate(float normalizedExertion) const;
        float ComputeTargetRespDepth(float normalizedExertion) const;

        // Proportional to RAW fitness, not EffectiveFitness, which already has fatigue subtracted.
        float FatigueScale() const;
        float AcuteFatigueMax() const;
        float LongTermFatigueMax() const;

        float EffectiveRestingHR() const;
        float CurrentHeartRate() const;
        float EffectiveFitness() const;
        float ContractilityExcess() const;
        float NormalizedExertion(float exertion) const;

        void UpdateExertion(PlayerState state, float delta);
        void AdvanceTimeSkip(float duration, float targetMets);
        void UpdateContractility(float delta);
        float ContractilityTarget() const;
        void UpdateAcuteFatigue(float exertion, float delta);
        void UpdateLongTermFatigue(float gameHoursDelta);
        void UpdateFitness(float exertion, float gameHoursDelta);
        void UpdateCurrentHeartRate(float delta);
        void UpdateRespiration(float delta);
        void UpdateRespDepth(float delta, float target);
        float ComputeTargetHeartRate(float exertion) const;

    private:
        SimulationSettings          m_Settings;
        SimulationModelCoefficients m_Coefficients;

        float m_TargetHeartRate = 0.0F;
        float m_FastHR = 0.0F;
        float m_SlowHR = 0.0F;
        float m_Exertion = 0.0F;
        float m_Adrenaline = 0.0F;
        float m_Contractility = 0.0F; // [0, 1]
        float m_Fitness = 0.0F;
        float m_AcuteFatigue = 0.0F;
        float m_LongTermFatigue = 0.0F;
        float m_RespRate = 0.0F;
        float m_RespDepth = 0.0F; // normalized tidal-volume state [0, 1]
        float m_RespPhase = 0.0F;

        std::optional<float> m_MaybeDeathSeconds;

        // Plain scalars: the mailbox makes the writer single, so these need no atomics.
        bool  m_DidJump = false;
        float m_SleepDuration = Sentinel;
        float m_FastTravelDuration = Sentinel;
        float m_WaitDuration = Sentinel;
    };
}
