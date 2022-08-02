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
#include "Logging.hpp"

#include "Config.hpp"

#ifdef SOURCE_LOCATION_IF_DEBUG
#error SOURCE_LOCATION_IF_DEBUG already defined.
#endif

#ifndef NDEBUG
#define SOURCE_LOCATION_IF_DEBUG "[%s:%#]"
#else
#define SOURCE_LOCATION_IF_DEBUG ""
#endif

namespace
{
    constexpr const char GlobalLoggerName[] = "GLOBAL";
}

void SHR::Logging::Init()
{
    std::optional<std::filesystem::path> maybePath = SKSE::log::log_directory();
    if (!maybePath)
    {
        SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory!");
    }

    std::filesystem::path path = std::exchange(maybePath, std::nullopt).value();
    path /= fmt::format(FMT_STRING("{:s}.log"), SKSE::PluginDeclaration::GetSingleton()->GetName());

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true));
    if (IsDebuggerPresent())
    {
        sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
    }

    auto logger = std::make_shared<spdlog::logger>(
        GlobalLoggerName,
        sinks.begin(),
        sinks.end()
    );

    const auto &debug = Config::Get().Debug;
    logger->set_level(debug.Log);
    logger->flush_on(debug.Flush);
    logger->set_pattern("[%Y-%m-%dT%T.%e][%n][%t][%L]" SOURCE_LOCATION_IF_DEBUG " %v");

    spdlog::set_default_logger(std::move(logger));
}
