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

#include "adapter/Config.hpp"
#include "adapter/HeartRate.hpp"

#include <cstddef>
#include <optional>
#include <string>

// Which message a notification shows; `draw` carries the randomness in, leaving selection pure.
// ARCHITECTURE.md (notifications) owns why selection is shaped this way.
namespace SHR::NotificationPolicy
{
    constexpr std::size_t RequiredPulseCount = 6;

    bool IsEnabled(const Notification &notification) noexcept;

    // The first pulse band holding no message, or nullopt when all six can answer. IsEnabled rejects
    // such a configuration; Config::Init calls this only to say which band is at fault.
    std::optional<std::size_t> FindEmptyBand(const Notification &notification) noexcept;

    // The pool entry a `draw` lands on.
    std::size_t SelectIndex(std::size_t poolSize, std::size_t draw) noexcept;

    // The same, refusing to land on `previous` while the pool offers an alternative.
    std::size_t SelectIndexExcluding(std::size_t poolSize, std::size_t draw, std::size_t previous) noexcept;

    // The message to show, or null when this configuration has none. It points into the caller's
    // configuration snapshot and lives exactly as long as that does.
    const std::string *SelectPulse(
        const Notification &notification,
        HeartRateLevel heartRateLevel,
        std::size_t draw
    ) noexcept;

    const std::string *SelectStatus(
        const Notification &notification,
        bool isDead,
        float heartRate,
        std::size_t draw
    ) noexcept;

    const std::string *SelectArrhythmia(
        const Notification &notification,
        std::size_t draw
    ) noexcept;
}
