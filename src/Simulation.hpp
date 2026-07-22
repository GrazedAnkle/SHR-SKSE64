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

#include <atomic>
#include <optional>

namespace SHR
{
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
        void Init();
        // realDelta is frame time in seconds; gameHoursDelta is elapsed in-game hours.
        void Step(PlayerState state, float realDelta, float gameHoursDelta = 0.0F);

        void NotifyJump();
        // Durations are real seconds.
        void NotifySleep(float duration);
        void NotifyFastTravel(float duration);
        void NotifyCombatEntry();
        void NotifyHit();

        // Restore co-save state and recompute derived targets. Zero fitness/fastHR and negative
        // contractility/respDepth select defaults for fields absent from older saves.
        void Restore(
            float heartRate,
            float exertion,
            float adrenaline = 0.0F,
            float fitness = 0.0F,
            float acuteFatigue = 0.0F,
            float longTermFatigue = 0.0F,
            float fastHR = 0.0F,
            float respRate = 0.0F,
            float contractility = -1.0F,
            float respDepth = -1.0F
        );

        float GetHeartRate() const;
        float GetFastHR() const;
        float GetExertion() const;
        float GetAdrenaline() const;
        float GetContractility() const;
        // Nonnegative excess over the contractility implied by current HR.
        float GetContractilityExcess() const;
        float GetFitness() const;
        float GetEffectiveFitness() const;
        float GetAcuteFatigue() const;
        float GetLongTermFatigue() const;
        float GetRespRate() const;
        float GetRespDepth() const;
        float GetRespPhase() const;
        std::optional<float> GetDeathSeconds() const;

    private:
        static constexpr float Sentinel = -1.0F;

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

        std::atomic_bool m_DidJump = false;
        std::atomic<float> m_SleepDuration = Sentinel;
        std::atomic<float> m_FastTravelDuration = Sentinel;

        float EffectiveRestingHR() const;
        float NormalizedExertion(float exertion) const;

        void UpdateExertion(PlayerState state, float delta);
        void UpdateContractility(float delta);
        float ContractilityTarget() const;
        void UpdateAcuteFatigue(float exertion, float delta);
        void UpdateLongTermFatigue(float gameHoursDelta);
        void UpdateFitness(float exertion, float gameHoursDelta);
        void UpdateCurrentHeartRate(float delta);
        void UpdateRespiration(float delta);
        void UpdateRespDepth(float delta, float target);
        static float ComputeTargetRespRate(float normalizedExertion);
        static float ComputeTargetRespDepth(float normalizedExertion);
        float ComputeTargetHeartRate(float exertion) const;
    };
}
