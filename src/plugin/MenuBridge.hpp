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

// The Papyrus side of the Mod Configuration Menu: the natives SHR_Native.psc declares, dispatched
// against the settings registry by control id. docs/MENU_ARTIFACTS.md owns the artifacts these
// bind to; ARCHITECTURE.md owns why the menu is the only writer of settings at runtime.

namespace SHR::MenuBridge
{
    // Queues the SHR_Native registration with the Papyrus VM. Must run before the VM starts binding
    // scripts, so kDataLoaded is already too late. Inert without a menu installed: nothing calls in.
    void Register();

    // Logs whether SHR.esp and its config quest loaded, and the quest's run state. The console
    // cannot answer this: SSE strips most editor IDs, so a failed lookup there does not distinguish
    // an absent form from an unresolvable name.
    //
    // Call from the ONE messaging listener in Main.cpp: SKSE keeps only the first listener a plugin
    // registers, and a second RegisterListener is accepted and then never dispatched to.
    void Report(const char *when);
}
