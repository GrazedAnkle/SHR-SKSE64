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
#include "adapter/NotificationPolicy.hpp"

#include <utility>

bool SHR::NotificationPolicy::IsEnabled(const Notification &notification) noexcept
{
    return notification.Enabled && notification.Pulse.size() >= RequiredPulseCount;
}

std::optional<std::string_view> SHR::NotificationPolicy::SelectPulse(
    const Notification &notification,
    HeartRateLevel heartRateLevel
) noexcept
{
    if (!IsEnabled(notification))
    {
        return std::nullopt;
    }

    static_assert(std::to_underlying(HeartRateLevel::Extreme) + 1 == RequiredPulseCount);
    return notification.Pulse[std::to_underlying(heartRateLevel)];
}

std::optional<std::string_view> SHR::NotificationPolicy::SelectStatus(
    const Notification &notification,
    bool isDead,
    float heartRate
) noexcept
{
    if (!IsEnabled(notification))
    {
        return std::nullopt;
    }

    if (isDead)
    {
        return heartRate == 0.0F ? notification.Dead : notification.Dying;
    }

    return SelectPulse(notification, GetHeartRateLevel(heartRate));
}

std::optional<std::string_view> SHR::NotificationPolicy::SelectArrhythmia(
    const Notification &notification
) noexcept
{
    if (!IsEnabled(notification))
    {
        return std::nullopt;
    }

    return notification.Arrhythmia;
}
