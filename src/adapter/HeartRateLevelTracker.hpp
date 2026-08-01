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

#include "adapter/HeartRate.hpp"

#include <optional>

namespace SHR
{
    // Decides *when* a status notification is due; NotificationPolicy owns *which* message it is.
    // Sampling cadence is the caller's: the adapter observes on beats, so a band crossed and
    // recrossed between two beats is deliberately never reported.
    class HeartRateLevelTracker
    {
    public:
        // The new level when this observation crosses into a different band, otherwise nullopt.
        std::optional<HeartRateLevel> Observe(float heartRate) noexcept
        {
            const HeartRateLevel level = GetHeartRateLevel(heartRate);
            if (level == m_Level)
            {
                return std::nullopt;
            }

            m_Level = level;
            return level;
        }

        HeartRateLevel Current() const noexcept
        {
            return m_Level;
        }

    private:
        // Also the band a character starts in, so a fresh tracker stays silent until rate leaves rest.
        HeartRateLevel m_Level = HeartRateLevel::Resting;
    };
}
