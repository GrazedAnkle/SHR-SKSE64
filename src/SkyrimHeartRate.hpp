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

        static constexpr float DeathSkipChanceIncreaseDuration = 180.0F;
        static constexpr float HeartRateFibrillation = 350.0F;

        static void InstallHooks(SKSE::Trampoline &trampoline);

        static void Init();

        static void NotifyJump();

        static void NotifySleep(float duration);

        static void NotifyFastTravel(float duration);

        static float GetHeartRate();

        static std::optional<Timestamp> GetDeathTimestamp();
    };
}
