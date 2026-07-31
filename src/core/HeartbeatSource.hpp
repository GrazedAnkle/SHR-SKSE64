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

#include "core/AudioBuffer.hpp"
#include "core/ModelCoefficients.hpp"

#include <cstddef>
#include <optional>

namespace SHR
{
    struct HeartbeatSourceSlices
    {
        AudioBuffer S1;
        AudioBuffer S2;
    };

    // Located on conditioned baseline S1, before any beat-specific effect is applied. These frames are
    // not landmarks in a compressed, filtered, resampled, or mixed beat.
    struct BaselineAttackRegion
    {
        std::size_t StartFrame;
        std::size_t PeakFrame;

        friend constexpr bool operator==(const BaselineAttackRegion &, const BaselineAttackRegion &) = default;
    };

    struct HeartbeatSource
    {
        AudioBuffer                         S1;
        AudioBuffer                         S2;
        std::optional<BaselineAttackRegion> S1BaselineAttack;
    };

    // These narrow stages are public so the representation migration can compare like with like. They
    // deliberately do not form a general-purpose DSP framework.
    HeartbeatSourceSlices SliceHeartbeatSource(ConstAudioBufferView decoded);

    void ApplyHeartbeatSourceHighPass(
        HeartbeatSourceSlices &source,
        float                  cutoffHz
    );

    void NormalizeHeartbeatSourceJoint(
        HeartbeatSourceSlices &source,
        float                  targetPeak
    );

    std::optional<BaselineAttackRegion> FindBaselineAttackRegion(
        ConstAudioBufferView input,
        float                thresholdFraction
    );

    HeartbeatSource PrepareHeartbeatSource(ConstAudioBufferView decoded);
    HeartbeatSource PrepareHeartbeatSource(
        ConstAudioBufferView                  decoded,
        const SourceConditioningCoefficients &coefficients
    );
}
