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
#include "Config.hpp"
#include "EventHandler.hpp"
#include "InputHandler.hpp"
#include "Logging.hpp"
#include "SkyrimHeartRate.hpp"
#include "Sound.hpp"

namespace
{
    void InitializeMessaging()
    {
        auto listener = [](SKSE::MessagingInterface::Message *message)
        {
            switch (message->type)
            {
            case SKSE::MessagingInterface::kDataLoaded:
                SHR::Sound::Init();
                SHR::InstallHooks();
                SHR::Config::Init();
                SHR::HeartRateManager::Init();
                SHR::EventHandler::Register();
                SHR::InputHandler::Register();
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
    SKSE::log::info(FMT_STRING("{:s} v{:s}"), plugin->GetName(), version.string("."));

    SKSE::Init(skse);
    InitializeMessaging();

    SKSE::log::info(FMT_STRING("{:s} loaded."), plugin->GetName());

    return true;
}
