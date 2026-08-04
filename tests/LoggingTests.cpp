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
#include "adapter/LoggingConfiguration.hpp"

#include <catch2/catch_test_macros.hpp>
#include <spdlog/sinks/ostream_sink.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    namespace fs = std::filesystem;

    class DefaultLoggerGuard
    {
    public:
        DefaultLoggerGuard() : m_Original(spdlog::default_logger()) { }

        ~DefaultLoggerGuard()
        {
            spdlog::set_default_logger(std::move(m_Original));
            SHR::Config::Set(SHR::Config{ });
        }

        DefaultLoggerGuard(const DefaultLoggerGuard &) = delete;
        DefaultLoggerGuard &operator=(const DefaultLoggerGuard &) = delete;

    private:
        std::shared_ptr<spdlog::logger> m_Original;
    };

    std::shared_ptr<spdlog::logger> InstallCaptureLogger(std::ostringstream &output)
    {
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(output);
        auto logger = std::make_shared<spdlog::logger>("TEST", std::move(sink));
        logger->set_pattern("%v");
        spdlog::set_default_logger(logger);
        return logger;
    }
}

TEST_CASE("Logging applies generated configuration defaults after bootstrap", "[logging][config]")
{
    DefaultLoggerGuard guard;
    std::ostringstream output;
    const auto logger = InstallCaptureLogger(output);
    SHR::Logging::Configure(SHR::Debug{ });

    const fs::path dir  = fs::temp_directory_path() / "shr_logging_default_config";
    const fs::path path = dir / "SHR.toml";
    fs::remove_all(dir);

    REQUIRE_NOTHROW(SHR::Config::Init(path.string()));
    SHR::Logging::Configure(SHR::Config::Get()->Debug);

    CHECK(output.str().contains("not found - generating defaults"));
    CHECK(logger->level() == spdlog::level::info);
    CHECK(logger->flush_level() == spdlog::level::trace);

    fs::remove_all(dir);
}

TEST_CASE("Configuration diagnostics bypass the configured runtime threshold", "[logging][config]")
{
    DefaultLoggerGuard guard;
    std::ostringstream output;
    const auto logger = InstallCaptureLogger(output);
    SHR::Logging::Configure(SHR::Debug{ });

    const fs::path dir  = fs::temp_directory_path() / "shr_logging_config_diagnostics";
    const fs::path path = dir / "SHR.toml";
    fs::remove_all(dir);
    fs::create_directories(dir);

    std::ofstream config(path);
    config << R"toml(
[debug]
log = "critical"
flush = "error"

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
    config.close();

    REQUIRE_NOTHROW(SHR::Config::Init(path.string()));
    CHECK(output.str().contains("pulse notification array requires at least"));

    SHR::Logging::Configure(SHR::Config::Get()->Debug);
    CHECK(logger->level() == spdlog::level::critical);
    CHECK(logger->flush_level() == spdlog::level::err);

    spdlog::error("runtime error should be filtered");
    spdlog::critical("runtime critical should be logged");
    CHECK_FALSE(output.str().contains("runtime error should be filtered"));
    CHECK(output.str().contains("runtime critical should be logged"));

    fs::remove_all(dir);
}
