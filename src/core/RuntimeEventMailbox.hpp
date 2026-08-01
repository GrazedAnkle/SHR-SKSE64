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

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace SHR
{
    enum class RuntimeEventKind
    {
        Jump,
        Sleep,
        FastTravel,
        CombatEntry,
        Hit,
    };

    struct RuntimeEvent
    {
        RuntimeEventKind Kind;
        // Real seconds. Meaningful for Sleep and FastTravel; ignored by the other kinds.
        float Duration = 0.0F;
    };

    // Many producer threads, one consumer. ARCHITECTURE.md (thread contract) owns which engine
    // callbacks arrive off the update thread and why every kind routes through here.
    class RuntimeEventMailbox
    {
    public:
        // One frame's worth with large headroom; the drain runs every stepped frame.
        static constexpr std::size_t Capacity = 64;

        // Any thread. Drops rather than blocks when full, so a producer never stalls a game thread.
        bool Post(const RuntimeEvent &event)
        {
            const std::scoped_lock lock(m_Mutex);
            if (m_Count == Capacity)
            {
                ++m_Dropped;
                return false;
            }

            m_Events[m_Count] = event;
            ++m_Count;
            return true;
        }

        // Copies pending events into out in post order and empties the mailbox, returning how many
        // were written. Never allocates, so the caller can reuse one buffer.
        std::size_t Drain(std::array<RuntimeEvent, Capacity> &out)
        {
            const std::scoped_lock lock(m_Mutex);
            const std::size_t count = m_Count;
            for (std::size_t i = 0; i < count; ++i)
            {
                out[i] = m_Events[i];
            }
            m_Count = 0;
            return count;
        }

        // Monotonic; non-zero means the drain is not keeping up. Core cannot log, so the adapter
        // surfaces it.
        std::uint64_t DroppedCount() const
        {
            const std::scoped_lock lock(m_Mutex);
            return m_Dropped;
        }

        void Clear()
        {
            const std::scoped_lock lock(m_Mutex);
            m_Count = 0;
        }

    private:
        mutable std::mutex                  m_Mutex;
        std::array<RuntimeEvent, Capacity>  m_Events = { };
        std::size_t                         m_Count   = 0;
        std::uint64_t                       m_Dropped = 0;
    };
}
