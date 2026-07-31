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
#include "adapter/Config.hpp"
#include "adapter/NotificationPolicy.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>
#include <fstream>

namespace
{
    namespace fs = std::filesystem;
    using Catch::Matchers::WithinAbs;
}

TEST_CASE("Config generates parseable defaults when the file is missing", "[config]")
{
    const fs::path dir  = fs::temp_directory_path() / "shr_config_roundtrip";
    const fs::path path = dir / "SHR.toml";
    fs::remove_all(dir);

    REQUIRE_FALSE(fs::exists(path));
    REQUIRE_NOTHROW(SHR::Config::Init(path.string()));
    REQUIRE(fs::exists(path));

    const SHR::Config defaults{ };
    const SHR::Config &loaded = SHR::Config::Get();

    CHECK(loaded.Debug.Log == defaults.Debug.Log);
    CHECK(loaded.Debug.Flush == defaults.Debug.Flush);
    CHECK_THAT(loaded.HeartRate.Resting, WithinAbs(defaults.HeartRate.Resting, 1e-6));
    CHECK_THAT(loaded.HeartRate.Max, WithinAbs(defaults.HeartRate.Max, 1e-6));
    CHECK_THAT(loaded.Arrhythmia.Susceptibility, WithinAbs(defaults.Arrhythmia.Susceptibility, 1e-6));
    CHECK(loaded.Input.Listen == defaults.Input.Listen);
    CHECK_THAT(loaded.Audio.Volume, WithinAbs(defaults.Audio.Volume, 1e-6));
    CHECK(loaded.Notification.Enabled == defaults.Notification.Enabled);
    CHECK(loaded.Notification.Arrhythmia == defaults.Notification.Arrhythmia);
    CHECK_FALSE(SHR::NotificationPolicy::IsEnabled(loaded.Notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(loaded.Notification, false, 80.0F));
    CHECK_FALSE(SHR::NotificationPolicy::SelectArrhythmia(loaded.Notification));

    // Re-init on the now-existing file must parse cleanly.
    REQUIRE_NOTHROW(SHR::Config::Init(path.string()));

    fs::remove_all(dir);
    SHR::Config::Set(SHR::Config{ }); // Restore global state for other tests.
}

TEST_CASE("Config disables notifications when an enabled pulse array is incomplete", "[config][notification]")
{
    const fs::path dir  = fs::temp_directory_path() / "shr_config_incomplete_notifications";
    const fs::path path = dir / "SHR.toml";
    fs::remove_all(dir);
    fs::create_directories(dir);

    std::ofstream out(path);
    out << R"toml(
[debug]
log = "info"
flush = "trace"

[heart_rate]
resting = 55.0
max = 200.0

[arrhythmia]
susceptibility = 1.0

[input]
listen = 35

[audio]
volume = 1.0

[notification]
enabled = true
pulse = ["only one"]
dying = "dying"
dead = "dead"
arrhythmia = "arrhythmia"
)toml";
    out.close();

    REQUIRE_NOTHROW(SHR::Config::Init(path.string()));

    const SHR::Notification &notification = SHR::Config::Get().Notification;
    CHECK_FALSE(notification.Enabled);
    CHECK_FALSE(SHR::NotificationPolicy::IsEnabled(notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, false, 80.0F));
    CHECK_FALSE(SHR::NotificationPolicy::SelectArrhythmia(notification));

    fs::remove_all(dir);
    SHR::Config::Set(SHR::Config{ }); // Restore global state for other tests.
}
