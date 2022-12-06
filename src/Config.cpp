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

#include <toml.hpp>

namespace
{
    SHR::Config s_Config;
}

namespace toml
{
    template<>
    struct from<SHR::Debug>
    {
        static SHR::Debug from_toml(const toml::value &value)
        {
            const auto log   = toml::find<std::string>(value, SHR::Debug::LogKey);
            const auto flush = toml::find<std::string>(value, SHR::Debug::FlushKey);

            return {
                .Log   = spdlog::level::from_str(log),
                .Flush = spdlog::level::from_str(flush),
            };
        }
    };

    template<>
    struct into<SHR::Debug>
    {
        static toml::value into_toml(const SHR::Debug &debug)
        {
            std::string log = fmt::to_string(spdlog::level::to_string_view(debug.Log));
            std::string flush = fmt::to_string(spdlog::level::to_string_view(debug.Flush));

            return toml::value{
                { SHR::Debug::LogKey, std::move(log)     },
                { SHR::Debug::FlushKey, std::move(flush) },
            };
        }
    };

    template<>
    struct from<SHR::Limit>
    {
        static SHR::Limit from_toml(const toml::value &value)
        {
            return {
                .Resting   = toml::find<float>(value, SHR::Limit::RestingKey),
                .Idle      = toml::find<float>(value, SHR::Limit::IdleKey),
                .Walking   = toml::find<float>(value, SHR::Limit::WalkingKey),
                .Running   = toml::find<float>(value, SHR::Limit::RunningKey),
                .Sprinting = toml::find<float>(value, SHR::Limit::SprintingKey),
                .Combat    = toml::find<float>(value, SHR::Limit::CombatKey),
            };
        }
    };

    template<>
    struct into<SHR::Limit>
    {
        static toml::value into_toml(const SHR::Limit &limit)
        {
            return toml::value{
                { SHR::Limit::RestingKey,   limit.Resting   },
                { SHR::Limit::IdleKey,      limit.Idle      },
                { SHR::Limit::WalkingKey,   limit.Walking   },
                { SHR::Limit::RunningKey,   limit.Running   },
                { SHR::Limit::SprintingKey, limit.Sprinting },
                { SHR::Limit::CombatKey,    limit.Combat    },
            };
        }
    };

    template<>
    struct from<SHR::Multiplier>
    {
        static SHR::Multiplier from_toml(const toml::value &value)
        {
            return {
                .Mod         = toml::find<float>(value, SHR::Multiplier::ModKey),
                .IncDecRatio = toml::find<float>(value, SHR::Multiplier::IncDecRatioKey),
                .SkipChance  = toml::find<float>(value, SHR::Multiplier::SkipChanceKey),
            };
        }
    };

    template<>
    struct into<SHR::Multiplier>
    {
        static toml::value into_toml(const SHR::Multiplier &multiplier)
        {
            return toml::value{
                { SHR::Multiplier::ModKey,         multiplier.Mod         },
                { SHR::Multiplier::IncDecRatioKey, multiplier.IncDecRatio },
                { SHR::Multiplier::SkipChanceKey,  multiplier.SkipChance  },
            };
        }
    };

    template<>
    struct from<SHR::Input>
    {
        static SHR::Input from_toml(const toml::value &value)
        {
            return {
                .Listen = toml::find<std::uint32_t>(value, SHR::Input::ListenKey),
            };
        }
    };

    template<>
    struct into<SHR::Input>
    {
        static toml::value into_toml(const SHR::Input &input)
        {
            return toml::value{
                { SHR::Input::ListenKey, input.Listen },
            };
        }
    };

    template<>
    struct from<SHR::Notification>
    {
        static SHR::Notification from_toml(const toml::value &value)
        {
            bool enabled = toml::find<bool>(value, SHR::Notification::EnabledKey);
            std::vector<std::string> pulse = toml::find<std::vector<std::string>>(value, SHR::Notification::PulseKey);

            constexpr std::size_t expectedPulseNotificationCount = 6;
            if (enabled && pulse.size() != expectedPulseNotificationCount)
            {
                // Disable notifications to avoid reading an invalid address.
                enabled = false;
                SKSE::log::error(
                    FMT_STRING("Expected {:d} pulse notifications; got {:d}"),
                    expectedPulseNotificationCount,
                    pulse.size()
                );
            }

            return {
                .Enabled      = enabled,
                .Pulse        = std::move(pulse),
                .Dying        = toml::find<std::string>(value, SHR::Notification::DyingKey),
                .Fibrillating = toml::find<std::string>(value, SHR::Notification::FibrillatingKey),
                .Dead         = toml::find<std::string>(value, SHR::Notification::DeadKey),
                .Skipped      = toml::find<std::string>(value, SHR::Notification::SkippedKey),
            };
        }
    };

    template<>
    struct into<SHR::Notification>
    {
        static toml::value into_toml(const SHR::Notification &binding)
        {
            return toml::value{
                { SHR::Notification::EnabledKey,      binding.Enabled      },
                { SHR::Notification::PulseKey,        binding.Pulse        },
                { SHR::Notification::DyingKey,        binding.Dying        },
                { SHR::Notification::FibrillatingKey, binding.Fibrillating },
                { SHR::Notification::DeadKey,         binding.Dead         },
                { SHR::Notification::SkippedKey,      binding.Skipped      },
            };
        }
    };

    template<>
    struct from<SHR::Config>
    {
        static SHR::Config from_toml(const toml::value &value)
        {
            return {
                .Debug        = toml::find<SHR::Debug>(value, SHR::Config::DebugKey),
                .Limit        = toml::find<SHR::Limit>(value, SHR::Config::LimitKey),
                .Multiplier   = toml::find<SHR::Multiplier>(value, SHR::Config::MultiplierKey),
                .Input        = toml::find<SHR::Input>(value, SHR::Config::InputKey),
                .Notification = toml::find<SHR::Notification>(value, SHR::Config::NotificationKey),
            };
        }
    };

    template<>
    struct into<SHR::Config>
    {
        static toml::value into_toml(const SHR::Config &config)
        {
            return toml::value{
                { SHR::Config::DebugKey,        config.Debug        },
                { SHR::Config::LimitKey,        config.Limit        },
                { SHR::Config::MultiplierKey,   config.Multiplier   },
                { SHR::Config::InputKey,        config.Input        },
                { SHR::Config::NotificationKey, config.Notification },
            };
        }
    };
}

const SHR::Config &SHR::Config::Get()
{
    return s_Config;
}

void SHR::Config::Init()
{
    const std::string_view pluginName = SKSE::PluginDeclaration::GetSingleton()->GetName();
    const std::string configPath = fmt::format(R"(Data\SKSE\Plugins\{}.toml)", pluginName);
    const auto data = toml::parse(configPath);
    s_Config = toml::from<Config>::from_toml(data);
}
