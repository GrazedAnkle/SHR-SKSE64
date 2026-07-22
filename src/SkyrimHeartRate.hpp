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

#include <SKSE/SKSE.h>

#include <chrono>

namespace SHR
{
    void InstallHooks();

    class HeartRateManager
    {
    public:
        using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;

        static constexpr float DeathArrhythmiaChanceIncreaseDuration = 2.0F * 60.0F;

        static void InstallHooks(SKSE::Trampoline &trampoline);

        static void Init();

        static void NotifyJump();

        static void NotifySleep(float duration);

        static void NotifyFastTravel(float duration);

        static void NotifyCombatEntry();

        static void NotifyHit();

        static float GetHeartRate();
    };
}
