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

#include <algorithm>
#include <utility>

namespace
{
    // A pool that cannot answer is silence rather than an empty message.
    const std::string *Draw(const SHR::MessagePool &pool, std::size_t draw) noexcept
    {
        if (pool.empty())
        {
            return nullptr;
        }

        return &pool[SHR::NotificationPolicy::SelectIndex(pool.size(), draw)];
    }
}

bool SHR::NotificationPolicy::IsEnabled(const Notification &notification) noexcept
{
    return notification.Enabled && notification.Pulse.size() >= RequiredPulseCount
        && !FindEmptyBand(notification);
}

std::optional<std::size_t> SHR::NotificationPolicy::FindEmptyBand(
    const Notification &notification
) noexcept
{
    // Bands past the sixth are never selected, so their contents are not this rule's business.
    const std::size_t bands = std::min(notification.Pulse.size(), RequiredPulseCount);
    for (std::size_t index = 0; index < bands; ++index)
    {
        if (notification.Pulse[index].empty())
        {
            return index;
        }
    }

    return std::nullopt;
}

std::size_t SHR::NotificationPolicy::SelectIndex(std::size_t poolSize, std::size_t draw) noexcept
{
    return poolSize == 0 ? 0 : draw % poolSize;
}

std::size_t SHR::NotificationPolicy::SelectIndexExcluding(
    std::size_t poolSize,
    std::size_t draw,
    std::size_t previous
) noexcept
{
    if (poolSize <= 1 || previous >= poolSize)
    {
        return SelectIndex(poolSize, draw);
    }

    // Draw from the pool with `previous` removed, then reinsert it.
    const std::size_t index = draw % (poolSize - 1);
    return index >= previous ? index + 1 : index;
}

const std::string *SHR::NotificationPolicy::SelectPulse(
    const Notification &notification,
    HeartRateLevel heartRateLevel,
    std::size_t draw
) noexcept
{
    if (!IsEnabled(notification))
    {
        return nullptr;
    }

    static_assert(std::to_underlying(HeartRateLevel::Extreme) + 1 == RequiredPulseCount);
    return Draw(notification.Pulse[std::to_underlying(heartRateLevel)], draw);
}

const std::string *SHR::NotificationPolicy::SelectStatus(
    const Notification &notification,
    bool isDead,
    float heartRate,
    std::size_t draw
) noexcept
{
    if (!IsEnabled(notification))
    {
        return nullptr;
    }

    if (isDead)
    {
        return Draw(heartRate == 0.0F ? notification.Dead : notification.Dying, draw);
    }

    return SelectPulse(notification, GetHeartRateLevel(heartRate), draw);
}

const std::string *SHR::NotificationPolicy::SelectArrhythmia(
    const Notification &notification,
    std::size_t draw
) noexcept
{
    if (!IsEnabled(notification))
    {
        return nullptr;
    }

    return Draw(notification.Arrhythmia, draw);
}
