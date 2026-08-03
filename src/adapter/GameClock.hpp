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

#include <optional>

namespace SHR
{
    // Differences samples of the in-game calendar into elapsed hours. Sampling cadence is the caller's;
    // ARCHITECTURE.md (runtime contract) owns why a reset rebases rather than carrying a reading over.
    class GameClock
    {
    public:
        // Marks the held reading stale: the next sample belongs to a timeline this one cannot span.
        void Reset() noexcept
        {
            m_LastHours.reset();
        }

        // Elapsed in-game hours, and zero for the first sample after a reset.
        float Consume(float currentHours) noexcept
        {
            const float delta = m_LastHours ? currentHours - *m_LastHours : 0.0F;
            m_LastHours = currentHours;
            return delta;
        }

    private:
        std::optional<float> m_LastHours;
    };
}
