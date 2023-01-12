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
    class EventHandler final
        : public RE::BSTEventSink<RE::TESSleepStartEvent>
        , public RE::BSTEventSink<RE::TESSleepStopEvent>
        , public RE::BSTEventSink<RE::TESFastTravelEndEvent>
    {
    public:
        static void Register();

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESSleepStartEvent *event,
            RE::BSTEventSource<RE::TESSleepStartEvent> *source
        ) override;

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESSleepStopEvent *event,
            RE::BSTEventSource<RE::TESSleepStopEvent> *source
        ) override;

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESFastTravelEndEvent *event,
            RE::BSTEventSource<RE::TESFastTravelEndEvent> *source
        ) override;

    private:
        float m_Timestamp = 0.0F;
    };
}
