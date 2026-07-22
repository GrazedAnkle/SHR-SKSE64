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

#include "Config.hpp"
#include "HeartRate.hpp"

#include <cstddef>
#include <optional>
#include <string_view>

namespace SHR::NotificationPolicy
{
    constexpr std::size_t RequiredPulseCount = 6;

    bool IsEnabled(const Notification &notification) noexcept;

    std::optional<std::string_view> SelectPulse(
        const Notification &notification,
        HeartRateLevel heartRateLevel
    ) noexcept;

    std::optional<std::string_view> SelectStatus(
        const Notification &notification,
        bool isDead,
        float heartRate
    ) noexcept;

    std::optional<std::string_view> SelectArrhythmia(const Notification &notification) noexcept;
}
