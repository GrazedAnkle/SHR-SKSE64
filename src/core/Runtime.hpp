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

#include "core/RhythmEngine.hpp"
#include "core/RuntimeEventMailbox.hpp"
#include "core/RuntimeSettings.hpp"
#include "core/Simulation.hpp"
#include "core/StepInput.hpp"
#include "core/StepResult.hpp"

#include <atomic>

namespace SHR
{
    class Runtime
    {
    public:
        explicit Runtime(RuntimeSettings settings);
        Runtime(RuntimeSettings settings, RhythmRandom random);
        Runtime(RuntimeSettings settings, ModelCoefficients coefficients);
        Runtime(
            RuntimeSettings   settings,
            RhythmRandom      random,
            ModelCoefficients coefficients
        );

        // Resets simulation and rhythm as one owned unit.
        void Init();
        StepResult Step(const StepInput &input);

        // Update thread. Validates the whole update before applying any part of it, returning false
        // untouched otherwise. Preserves simulation and rhythm.
        bool ApplySettings(RuntimeSettings settings);
        const RuntimeSettings &GetSettings() const noexcept { return m_Settings; }

        // Any thread. Posts to the mailbox; takes effect at the top of the next Step, so a
        // notification is not visible in GetState until then.
        void NotifyJump();
        void NotifySleep(float duration);
        void NotifyFastTravel(float duration);
        void NotifyCombatEntry();
        void NotifyHit();

        // Expected to stay zero.
        std::uint64_t GetDroppedEventCount() const;

        // Update thread only: a wide non-atomic copy, so it tears if read during a step.
        PhysiologySnapshot GetSnapshot() const;

        // The last completed step's heart rate. The one physiology value read off the update thread.
        float GetPublishedHeartRate() const noexcept;
        float GetTargetHeartRate() const noexcept;
        float GetTargetRespirationRate() const;
        float GetTargetRespirationDepth() const;
        SimulationState GetState() const;
        SimulationState CreateInitialState() const;
        float ComputeEquilibriumContractility(const SimulationState &state) const;
        // SimulationState does not persist rhythm; Init owns rhythm lifecycle resets.
        void Restore(const SimulationState &state);

    private:
        void DrainEvents();

        RuntimeSettings   m_Settings;
        ModelCoefficients m_Coefficients;
        HeartRateSimulation     m_Simulation;
        RhythmEngine            m_Rhythm;
        RuntimeEventMailbox     m_Events;
        std::atomic<float>      m_PublishedHeartRate = 0.0F;

        // Reused so the drain never allocates.
        std::array<RuntimeEvent, RuntimeEventMailbox::Capacity> m_DrainBuffer = { };
    };
}
