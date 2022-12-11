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

#include "Common.hpp"
#include "Config.hpp"

namespace SHR
{
    void InstallHooks();

    class HeartRateManager
    {
    public:
        using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;

        static constexpr float DeathSkipChanceIncreaseDuration = 2.0F * 60.0F;
        static constexpr float HeartRateFibrillation = 350.0F;

        static void InstallHooks(SKSE::Trampoline &trampoline);

        static void Init();

        static void NotifyJump();

        static void NotifySleep(float duration);

        static void NotifyFastTravel(float duration);

        static float GetHeartRate();
    };

    enum class HeartRateLevel
    {
        Resting,
        Idle,
        Elevated,
        High,
        VeryHigh,
        Extreme,
    };

    constexpr float IdleHeartRateThreshold = 60.0F;
    constexpr float ElevatedHeartRateThreshold = 130.0F;
    constexpr float HighHeartRateThreshold = 150.0F;
    constexpr float VeryHighHeartRateThreshold = 170.0F;
    constexpr float ExtremeHeartRateThreshold = 190.0F;

    constexpr HeartRateLevel GetHeartRateLevel(float heartRate)
    {
        if (heartRate > ExtremeHeartRateThreshold)
        {
            return HeartRateLevel::Extreme;
        }
        else if (heartRate > VeryHighHeartRateThreshold)
        {
            return HeartRateLevel::VeryHigh;
        }
        else if (heartRate > HighHeartRateThreshold)
        {
            return HeartRateLevel::High;
        }
        else if (heartRate > ElevatedHeartRateThreshold)
        {
            return HeartRateLevel::Elevated;
        }
        else if (heartRate > IdleHeartRateThreshold)
        {
            return HeartRateLevel::Idle;
        }
        else
        {
            return HeartRateLevel::Resting;
        }
    }
}
