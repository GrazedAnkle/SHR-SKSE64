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
#include "core/RuntimeSettings.hpp"
#include "core/Simulation.hpp"
#include "core/StepInput.hpp"
#include "core/StepResult.hpp"

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

        void NotifyJump();
        void NotifySleep(float duration);
        void NotifyFastTravel(float duration);
        void NotifyCombatEntry();
        void NotifyHit();

        PhysiologySnapshot GetSnapshot() const;
        float GetTargetHeartRate() const noexcept;
        float GetTargetRespirationRate() const;
        float GetTargetRespirationDepth() const;
        SimulationState GetState() const;
        SimulationState CreateInitialState() const;
        float ComputeEquilibriumContractility(const SimulationState &state) const;
        // SimulationState does not persist rhythm; Init owns rhythm lifecycle resets.
        void Restore(const SimulationState &state);

    private:
        const RuntimeSettings   m_Settings;
        const ModelCoefficients m_Coefficients;
        HeartRateSimulation     m_Simulation;
        RhythmEngine            m_Rhythm;
    };
}
