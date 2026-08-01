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
#include "plugin/EventHandler.hpp"

#include "core/Constants.hpp"
#include "plugin/SkyrimHeartRate.hpp"
#include "plugin/ThreadTrace.hpp"

namespace
{
    namespace C = SHR::Constants;

    SHR::EventHandler s_EventHandler;
}

void SHR::EventHandler::Register()
{
    auto *eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
    eventSourceHolder->AddEventSink<RE::TESSleepStartEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESSleepStopEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESFastTravelEndEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESCombatEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESHitEvent>(&s_EventHandler);
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESSleepStartEvent *event,
    RE::BSTEventSource<RE::TESSleepStartEvent> *source
)
{
    SHR_TRACE_THREAD("sink.TESSleepStartEvent");

    const float currentTime = RE::Calendar::GetSingleton()->GetHoursPassed();
    m_Timestamp = currentTime;
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESSleepStopEvent *event,
    RE::BSTEventSource<RE::TESSleepStopEvent> *source
)
{
    SHR_TRACE_THREAD("sink.TESSleepStopEvent");

    const float currentTime = RE::Calendar::GetSingleton()->GetHoursPassed();
    const float durationHours = currentTime - m_Timestamp;
    m_Timestamp = currentTime;
    HeartRateManager::NotifySleep(durationHours * C::SecondsPerHour);
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESFastTravelEndEvent *event,
    RE::BSTEventSource<RE::TESFastTravelEndEvent> *source
)
{
    SHR_TRACE_THREAD("sink.TESFastTravelEndEvent");

    HeartRateManager::NotifyFastTravel(event->fastTravelEndHours * C::SecondsPerHour);
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESCombatEvent *event,
    RE::BSTEventSource<RE::TESCombatEvent> *source
)
{
    SHR_TRACE_THREAD("sink.TESCombatEvent");

    const auto *player = RE::PlayerCharacter::GetSingleton();
    if (event->actor.get() == player && event->newState != RE::ACTOR_COMBAT_STATE::kNone)
    {
        HeartRateManager::NotifyCombatEntry();
    }
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESHitEvent *event,
    RE::BSTEventSource<RE::TESHitEvent> *source
)
{
    SHR_TRACE_THREAD("sink.TESHitEvent");

    const auto *player = RE::PlayerCharacter::GetSingleton();
    if (event->target.get() == player)
    {
        HeartRateManager::NotifyHit();
    }
    return RE::BSEventNotifyControl::kContinue;
}
