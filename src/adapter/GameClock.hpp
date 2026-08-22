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
#include <cmath>
#include <limits>
#include <optional>

namespace SHR
{
    // Differences samples of the in-game calendar into elapsed hours. Sampling cadence is the caller's;
    // ARCHITECTURE.md (runtime contract) owns why a reset rebases rather than carrying a reading over.
    //
    // Reset and Consume belong to the update thread; Peek is readable from any. The held reading is
    // atomic because ARCHITECTURE.md (thread contract) records sink delivery as measured rather than
    // guaranteed, so Peek's caller cannot rely on landing where Consume does.
    class GameClock
    {
    public:
        // Marks the held reading stale: the next sample belongs to a timeline this one cannot span.
        void Reset() noexcept
        {
            m_LastHours.store(Unset, std::memory_order_relaxed);
        }

        // Elapsed in-game hours, and zero for the first sample after a reset.
        float Consume(float currentHours) noexcept
        {
            const float held = m_LastHours.load(std::memory_order_relaxed);
            const float delta = std::isnan(held) ? 0.0F : currentHours - held;
            m_LastHours.store(currentHours, std::memory_order_relaxed);
            return delta;
        }

        // The held reading without consuming it. Empty after a reset and before the first sample.
        std::optional<float> Peek() const noexcept
        {
            const float held = m_LastHours.load(std::memory_order_relaxed);
            if (std::isnan(held))
            {
                return std::nullopt;
            }
            return held;
        }

    private:
        // NaN sentinel rather than a second atomic, which would need the two kept consistent.
        static constexpr float Unset = std::numeric_limits<float>::quiet_NaN();

        std::atomic<float> m_LastHours{ Unset };
    };
}
