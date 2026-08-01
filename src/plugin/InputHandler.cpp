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
#include "plugin/InputHandler.hpp"

#include "adapter/Config.hpp"
#include "adapter/NotificationPolicy.hpp"
#include "plugin/SkyrimHeartRate.hpp"
#include "plugin/ThreadTrace.hpp"

namespace
{
    std::atomic_int s_IsListening = 0;
}

void SHR::InputHandler::Register()
{
    auto *deviceManager = RE::BSInputDeviceManager::GetSingleton();
    deviceManager->AddEventSink(&GetInstance());
}

bool SHR::InputHandler::IsListening() noexcept
{
    return s_IsListening.load() != 0;
}

RE::BSEventNotifyControl SHR::InputHandler::ProcessEvent(
    RE::InputEvent *const *event,
    RE::BSTEventSource<RE::InputEvent *> *source
)
{
    SHR_TRACE_THREAD("sink.InputEvent");

    if (!event || RE::UI::GetSingleton()->GameIsPaused())
    {
        return RE::BSEventNotifyControl::kContinue;
    }

    for (const RE::InputEvent *it = *event; it != nullptr; it = it->next)
    {
        if (it->GetEventType() != RE::INPUT_EVENT_TYPE::kButton)
        {
            continue;
        }

        const RE::ButtonEvent *buttonEvent = it->AsButtonEvent();
        const std::uint32_t keyCode = buttonEvent->idCode;
        if (keyCode == Config::Get().Input.Listen)
        {
            if (buttonEvent->IsDown())
            {
                s_IsListening.fetch_xor(1);

                const auto notification = NotificationPolicy::SelectStatus(
                    Config::Get().Notification,
                    RE::PlayerCharacter::GetSingleton()->IsDead(),
                    HeartRateManager::GetHeartRate()
                );
                if (notification)
                {
                    RE::SendHUDMessage::ShowHUDMessage(notification->data());
                }
            }
        }
        else if (keyCode == RE::ControlMap::GetSingleton()->GetMappedKey("Jump", it->GetDevice()))
        {
            if (buttonEvent->IsDown())
            {
                HeartRateManager::NotifyJump();
            }
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}
