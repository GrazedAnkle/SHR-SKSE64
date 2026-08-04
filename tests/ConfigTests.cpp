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
#include <iterator>
#include <string>

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
    const auto loaded = SHR::Config::Get();

    CHECK(loaded->Debug.Log == defaults.Debug.Log);
    CHECK(loaded->Debug.Flush == defaults.Debug.Flush);
    CHECK_THAT(loaded->HeartRate.Resting, WithinAbs(defaults.HeartRate.Resting, 1e-6));
    CHECK_THAT(loaded->HeartRate.Max, WithinAbs(defaults.HeartRate.Max, 1e-6));
    CHECK_THAT(loaded->Arrhythmia.Susceptibility, WithinAbs(defaults.Arrhythmia.Susceptibility, 1e-6));
    CHECK(loaded->Input.Listen == defaults.Input.Listen);
    CHECK_THAT(loaded->Audio.Volume, WithinAbs(defaults.Audio.Volume, 1e-6));
    CHECK(loaded->Notification.Enabled == defaults.Notification.Enabled);
    CHECK(loaded->Notification.Arrhythmia == defaults.Notification.Arrhythmia);
    CHECK_FALSE(SHR::NotificationPolicy::IsEnabled(loaded->Notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(loaded->Notification, false, 80.0F));
    CHECK_FALSE(SHR::NotificationPolicy::SelectArrhythmia(loaded->Notification));

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

    const auto config = SHR::Config::Get();
    const SHR::Notification &notification = config->Notification;
    CHECK_FALSE(notification.Enabled);
    CHECK_FALSE(SHR::NotificationPolicy::IsEnabled(notification));
    CHECK_FALSE(SHR::NotificationPolicy::SelectStatus(notification, false, 80.0F));
    CHECK_FALSE(SHR::NotificationPolicy::SelectArrhythmia(notification));

    fs::remove_all(dir);
    SHR::Config::Set(SHR::Config{ }); // Restore global state for other tests.
}

TEST_CASE("A held config snapshot survives a later write", "[config][settings]")
{
    SHR::Config first;
    first.Input.Listen       = 0x11;
    first.Notification.Dying = "old";
    SHR::Config::Set(first);

    const auto held = SHR::Config::Get();

    SHR::Config second;
    second.Input.Listen       = 0x22;
    second.Notification.Dying = "new";
    SHR::Config::Set(second);

    CHECK(held->Input.Listen == 0x11);
    CHECK(held->Notification.Dying == "old");

    CHECK(SHR::Config::Get()->Input.Listen == 0x22);
    CHECK(SHR::Config::Get()->Notification.Dying == "new");
}

TEST_CASE("A config written before the fitness ceiling existed still loads", "[config][settings]")
{
    const fs::path dir  = fs::temp_directory_path() / "shr_config_no_ceiling";
    const fs::path path = dir / "SHR.toml";
    fs::remove_all(dir);
    fs::create_directories(dir);

    std::ofstream out(path);
    out << R"toml(
[debug]
log = "info"
flush = "trace"

[heart_rate]
resting = 48.0
max = 195.0

[arrhythmia]
susceptibility = 1.0

[input]
listen = 35

[notification]
enabled = false
pulse = []
dying = "dying"
dead = "dead"
arrhythmia = "arrhythmia"
)toml";
    out.close();

    REQUIRE_NOTHROW(SHR::Config::Init(path.string()));

    const auto config = SHR::Config::Get();
    CHECK_THAT(config->HeartRate.Resting, WithinAbs(48.0F, 1e-6));
    CHECK_THAT(
        config->HeartRate.FitnessCeiling,
        WithinAbs(SHR::HeartRate{ }.FitnessCeiling, 1e-6)
    );
}

TEST_CASE("Persisting writes the menu-owned settings back without disturbing the file", "[config][settings]")
{
    const fs::path dir  = fs::temp_directory_path() / "shr_config_persist";
    const fs::path path = dir / "SHR.toml";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // Hand-written: comments, a key the menu never touches, and an ordering of its author's choosing.
    {
        std::ofstream out(path);
        out << "# My own notes, kept across a write.\n"
               "[debug]\n"
               "log = \"info\"\n"
               "flush = \"trace\"\n"
               "[heart_rate]\n"
               "# Tuned by hand.\n"
               "resting = 47.0\n"
               "max = 190.0\n"
               "[arrhythmia]\n"
               "susceptibility = 2.0\n"
               "[input]\n"
               "# Bound to L.\n"
               "listen = 38\n"
               "[audio]\n"
               "volume = 0.8\n"
               "[notification]\n"
               "enabled = false\n"
               "pulse = [\"a\", \"b\", \"c\", \"d\", \"e\", \"f\"]\n"
               "dying = \"dying\"\n"
               "dead = \"dead\"\n"
               "arrhythmia = \"pvc\"\n";
    }

    REQUIRE_NOTHROW(SHR::Config::Init(path.string()));

    SHR::Config edited = *SHR::Config::Get();
    edited.Audio.Volume         = 0.25F;
    edited.Input.Listen         = 42;
    edited.Notification.Enabled = true;
    SHR::Config::Set(edited);
    REQUIRE(SHR::Config::Persist());

    const std::string written = [&] {
        std::ifstream in(path);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }();

    // The player's prose is theirs; a settings write must not eat it.
    CHECK(written.find("My own notes") != std::string::npos);
    CHECK(written.find("Tuned by hand") != std::string::npos);
    CHECK(written.find("Bound to L") != std::string::npos);

    // Reloading is the real assertion: the values survive a restart, and the untouched ones are
    // still whatever the file said rather than a compiled default.
    REQUIRE_NOTHROW(SHR::Config::Init(path.string()));
    const auto reloaded = SHR::Config::Get();

    CHECK_THAT(reloaded->Audio.Volume, WithinAbs(0.25F, 1e-6));
    CHECK(reloaded->Input.Listen == 42);
    CHECK(reloaded->Notification.Enabled == true);

    CHECK_THAT(reloaded->HeartRate.Resting, WithinAbs(47.0F, 1e-6));
    CHECK_THAT(reloaded->HeartRate.Max, WithinAbs(190.0F, 1e-6));
    CHECK_THAT(reloaded->Arrhythmia.Susceptibility, WithinAbs(2.0F, 1e-6));
    CHECK(reloaded->Notification.Arrhythmia == "pvc");

    fs::remove_all(dir);
    SHR::Config::Set(SHR::Config{ }); // Restore global state for other tests.
}
