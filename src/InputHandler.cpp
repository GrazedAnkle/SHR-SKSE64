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
#include "InputHandler.hpp"

#include "Config.hpp"
#include "Random.hpp"
#include "SkyrimHeartRate.hpp"
#include "Sound.hpp"
#include "Strings.hpp"

namespace
{
    std::chrono::time_point<std::chrono::steady_clock> s_PreviousTime;

    float s_SkipDuration = 0.0F;

    bool s_ShouldSkip = false;
    bool s_DidJustSkip = false;
}

void SHR::InputHandler::Register()
{
    auto *deviceManager = RE::BSInputDeviceManager::GetSingleton();
    deviceManager->AddEventSink(&GetInstance());
}

RE::BSEventNotifyControl SHR::InputHandler::ProcessEvent(
    RE::InputEvent *const *event,
    RE::BSTEventSource<RE::InputEvent *> *source
)
{
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

        const auto *buttonEvent = it->AsButtonEvent();
        const std::uint32_t keyCode = buttonEvent->idCode;
        if (keyCode == Config::Get().Input.Listen)
        {
            if (buttonEvent->IsDown())
            {
                if (Config::Get().Notification.Enabled)
                {
                    RE::DebugNotification(
                        SHR::Strings::GetText(
                            RE::PlayerCharacter::GetSingleton(),
                            SHR::HeartRateManager::GetHeartRate()
                        )
                    );
                }
            }
            else if (buttonEvent->IsHeld())
            {
                const auto currentTime = std::chrono::steady_clock::now();

                const std::chrono::duration<float> deltaTime = currentTime - s_PreviousTime;
                const float seconds = deltaTime.count();
                float deathSeconds = -1.0F;
                if (const auto maybeDeathTimestamp = HeartRateManager::GetDeathTimestamp())
                {
                    const auto deathTimestamp = maybeDeathTimestamp.value();
                    const std::chrono::duration<float> deathDuration = currentTime - deathTimestamp;
                    deathSeconds = deathDuration.count();
                }

                constexpr float maxSkipChanceAfterSeconds = 2.0F * HeartRateManager::DeathSkipChanceIncreaseDuration;

                const float skipChanceFactor = deathSeconds == -1.0F
                    ? 0.0F
                    : std::min(deathSeconds, maxSkipChanceAfterSeconds) / maxSkipChanceAfterSeconds;

                constexpr float normalSkipChance = 0.00001F;
                constexpr float maxSkipChance = 0.001F;

                const float skipMultiplier = Config::Get().Multiplier.SkipChance;
                const float skipChance = skipMultiplier * std::lerp(normalSkipChance, maxSkipChance, skipChanceFactor);
                s_ShouldSkip = s_ShouldSkip || !s_DidJustSkip && Random(0.0F, 1.0F) < skipChance;

                const float heartRate = SHR::HeartRateManager::GetHeartRate();
                const float period = 60.0F / heartRate;

                constexpr float skipPeriodFraction = 0.7F;
                const float effectivePeriod = s_ShouldSkip ? skipPeriodFraction * period : period;
                if ((s_DidJustSkip && seconds < s_SkipDuration) || (seconds < effectivePeriod))
                {
                    continue;
                }

                const float effectiveHeartRate = s_ShouldSkip
                    ? heartRate / skipPeriodFraction
                    : heartRate;

                s_DidJustSkip = s_ShouldSkip;
                if (s_ShouldSkip)
                {
                    s_ShouldSkip = false;
                    s_SkipDuration = Random(0.75F, 1.5F);

                    if (Config::Get().Notification.Enabled)
                    {
                        RE::DebugNotification(Strings::GetSkippedText());
                    }
                }

                s_PreviousTime = currentTime;

                ::Sound::Play(SHR::Sound::GetDescriptor(effectiveHeartRate), RE::PlayerCharacter::GetSingleton());
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
