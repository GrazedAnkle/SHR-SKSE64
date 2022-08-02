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
#include "EventHandler.hpp"

#include "SkyrimHeartRate.hpp"

namespace
{
    SHR::EventHandler s_EventHandler;
}

void SHR::EventHandler::Register()
{
    auto *eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
    eventSourceHolder->AddEventSink<RE::TESSleepStartEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESSleepStopEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESFastTravelEndEvent>(&s_EventHandler);
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESSleepStartEvent *event,
    RE::BSTEventSource<RE::TESSleepStartEvent> *source
)
{
    const float currentTime = RE::Calendar::GetSingleton()->GetHoursPassed();
    m_Timestamp = currentTime;
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESSleepStopEvent *event,
    RE::BSTEventSource<RE::TESSleepStopEvent> *source
)
{
    const float currentTime = RE::Calendar::GetSingleton()->GetHoursPassed();
    const float duration = currentTime - m_Timestamp;
    m_Timestamp = currentTime;
    HeartRateManager::NotifySleep(duration);
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESFastTravelEndEvent *event,
    RE::BSTEventSource<RE::TESFastTravelEndEvent> *source
)
{
    const float duration = event->fastTravelEndHours;
    HeartRateManager::NotifyFastTravel(duration);
    return RE::BSEventNotifyControl::kContinue;
}
