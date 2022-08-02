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
#include "Strings.hpp"

#include "SkyrimHeartRate.hpp"

// TODO (2022-08-01): May need to copy strings to avoid use-after-free once config hot-reloading is implemented.
const char *SHR::Strings::GetText(const RE::PlayerCharacter *player, float heartRate)
{
    if (player->IsDead())
    {
        if (heartRate == 0.0F)
        {
            return Config::Get().Notification.Dead.c_str();
        }

        if (heartRate > SHR::HeartRateManager::HeartRateFibrillation)
        {
            return Config::Get().Notification.Fibrillating.c_str();
        }

        return Config::Get().Notification.Dying.c_str();
    }

    const std::vector<std::string> &notifications = Config::Get().Notification.Pulse;

    if (heartRate > 190.0F)
    {
        return notifications[5].c_str();
    }
    else if (heartRate > 170.0F)
    {
        return notifications[4].c_str();
    }
    else if (heartRate > 150.0F)
    {
        return notifications[3].c_str();
    }
    else if (heartRate > 130.0F)
    {
        return notifications[2].c_str();
    }
    else if (heartRate > 60.0F)
    {
        return notifications[1].c_str();
    }
    else
    {
        return notifications[0].c_str();
    }
}

const char *SHR::Strings::GetSkippedText()
{
    return Config::Get().Notification.Skipped.c_str();
}
