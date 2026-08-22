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
#include "plugin/PluginState.hpp"
#include "plugin/SkyrimHeartRate.hpp"

namespace
{
    namespace C = SHR::Constants;

    SHR::EventHandler s_EventHandler;

    // The calendar the game clock has not consumed, which across a paused skip is the interval no
    // frame simulated. Zero before the clock's first sample: there is nothing to difference against.
    float SkippedSeconds()
    {
        const float currentHours = RE::Calendar::GetSingleton()->GetHoursPassed();
        const std::optional<float> held = SHR::PluginState::Get().PeekGameHours();
        if (!held)
        {
            return 0.0F;
        }
        return std::max(currentHours - *held, 0.0F) * C::SecondsPerHour;
    }
}

void SHR::EventHandler::Register()
{
    auto *eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
    eventSourceHolder->AddEventSink<RE::TESSleepStopEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESWaitStopEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESFastTravelEndEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESCombatEvent>(&s_EventHandler);
    eventSourceHolder->AddEventSink<RE::TESHitEvent>(&s_EventHandler);
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESSleepStopEvent *event,
    RE::BSTEventSource<RE::TESSleepStopEvent> *source
)
{
    HeartRateManager::NotifySleep(SkippedSeconds());
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESWaitStopEvent *event,
    RE::BSTEventSource<RE::TESWaitStopEvent> *source
)
{
    HeartRateManager::NotifyWait(SkippedSeconds());
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESFastTravelEndEvent *event,
    RE::BSTEventSource<RE::TESFastTravelEndEvent> *source
)
{
    // event->fastTravelEndHours carries the same duration despite its name, but every skip reads
    // the clock so one rule covers them all.
    HeartRateManager::NotifyFastTravel(SkippedSeconds());
    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl SHR::EventHandler::ProcessEvent(
    const RE::TESCombatEvent *event,
    RE::BSTEventSource<RE::TESCombatEvent> *source
)
{
    const auto *player = RE::PlayerCharacter::GetSingleton();
    if (event->actor.get() != player)
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Searching counts as engaged: the player is still in the encounter while hunting a lost target.
    const bool engaged = event->newState != RE::ACTOR_COMBAT_STATE::kNone;
    if (m_Combat.Observe(engaged))
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
    const auto *player = RE::PlayerCharacter::GetSingleton();
    if (event->target.get() == player)
    {
        HeartRateManager::NotifyHit();
    }
    return RE::BSEventNotifyControl::kContinue;
}
