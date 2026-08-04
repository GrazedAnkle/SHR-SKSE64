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
#include "plugin/EventHandler.hpp"
#include "plugin/InputHandler.hpp"
#include "plugin/Logging.hpp"
#include "plugin/MenuBridge.hpp"
#include "plugin/SkyrimHeartRate.hpp"

namespace
{
    void InitializeMessaging()
    {
        auto listener = [](SKSE::MessagingInterface::Message *message)
        {
            switch (message->type)
            {
            case SKSE::MessagingInterface::kDataLoaded:
                {
                    const std::string configPath = fmt::format(
                        R"(Data\SKSE\Plugins\{}.toml)",
                        SKSE::PluginDeclaration::GetSingleton()->GetName()
                    );
                    SHR::InstallHooks();
                    // Keep all configuration diagnostics on the bootstrap logger.
                    SHR::Config::Init(configPath);
                    SHR::Logging::Configure(SHR::Config::Get()->Debug);
                    SHR::HeartRateManager::Init();
                    SHR::EventHandler::Register();
                    SHR::InputHandler::Register();
                    SHR::MenuBridge::Report("kDataLoaded");
                    break;
                }
            case SKSE::MessagingInterface::kNewGame:
                SHR::MenuBridge::Report("kNewGame");
                break;
            case SKSE::MessagingInterface::kPostLoadGame:
                SHR::MenuBridge::Report("kPostLoadGame");
                break;
            default:
                break;
            }
        };

        const auto *messagingInterface = SKSE::GetMessagingInterface();
        if (!messagingInterface->RegisterListener(listener))
        {
            SKSE::stl::report_and_fail("Failed to register message listener!");
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface *skse)
{
#ifndef NDEBUG
    while (!IsDebuggerPresent())
    {
    }
#endif

    SHR::Logging::Init();

    const auto *plugin = SKSE::PluginDeclaration::GetSingleton();
    const REL::Version version = plugin->GetVersion();
    const std::string versionString = version.string(".");
    SKSE::log::info(FMT_STRING("{:s} v{:s}"), plugin->GetName(), versionString);

    SKSE::Init(skse);
    InitializeMessaging();

    // Papyrus registrations are queued, so this must run before the VM starts binding scripts —
    // kDataLoaded is already too late.
    SHR::MenuBridge::Register();

    SKSE::log::info(FMT_STRING("{:s} loaded."), plugin->GetName());

    return true;
}
