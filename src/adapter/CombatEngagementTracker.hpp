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

namespace SHR
{
    // Decides *when* a combat-entry impulse is due. The engine reports a combat state rather than an
    // entry, and reports it repeatedly within one encounter, so only the edge into engagement counts.
    class CombatEngagementTracker
    {
    public:
        // True only on the edge, so a run of engaged observations yields one entry.
        bool Observe(bool engaged) noexcept
        {
            const bool entered = engaged && !m_Engaged;
            m_Engaged = engaged;
            return entered;
        }

        bool Engaged() const noexcept
        {
            return m_Engaged;
        }

    private:
        bool m_Engaged = false;
    };
}
