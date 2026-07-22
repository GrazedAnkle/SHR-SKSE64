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

#include "NotificationPolicy.hpp"

#include <toml.hpp>

#include <filesystem>
#include <fstream>

namespace
{
    SHR::Config s_Config;

    std::string LevelName(spdlog::level::level_enum level)
    {
        const auto view = spdlog::level::to_string_view(level);
        return std::string(view.data(), view.size());
    }
}

namespace toml
{
    template<>
    struct from<SHR::Debug>
    {
        static SHR::Debug from_toml(const toml::value &value)
        {
            return {
                .Log   = spdlog::level::from_str(toml::find<std::string>(value, SHR::Debug::LogKey)),
                .Flush = spdlog::level::from_str(toml::find<std::string>(value, SHR::Debug::FlushKey)),
            };
        }
    };

    template<>
    struct into<SHR::Debug>
    {
        template<typename TC>
        static basic_value<TC> into_toml(const SHR::Debug &debug)
        {
            basic_value<TC> value(typename basic_value<TC>::table_type{ });
            value[SHR::Debug::LogKey]   = LevelName(debug.Log);
            value[SHR::Debug::FlushKey] = LevelName(debug.Flush);
            return value;
        }
    };

    template<>
    struct from<SHR::HeartRate>
    {
        static SHR::HeartRate from_toml(const toml::value &value)
        {
            return {
                .Resting = toml::find<float>(value, SHR::HeartRate::RestingKey),
                .Max     = toml::find<float>(value, SHR::HeartRate::MaxKey),
            };
        }
    };

    template<>
    struct into<SHR::HeartRate>
    {
        template<typename TC>
        static basic_value<TC> into_toml(const SHR::HeartRate &heartRate)
        {
            basic_value<TC> resting(static_cast<double>(heartRate.Resting));
            resting.comments().push_back(" Initial resting heart rate; also initializes fitness.");

            basic_value<TC> value(typename basic_value<TC>::table_type{ });
            value[SHR::HeartRate::RestingKey] = resting;
            value[SHR::HeartRate::MaxKey]     = static_cast<double>(heartRate.Max);
            return value;
        }
    };

    template<>
    struct from<SHR::Arrhythmia>
    {
        static SHR::Arrhythmia from_toml(const toml::value &value)
        {
            return {
                .Susceptibility = toml::find<float>(value, SHR::Arrhythmia::SusceptibilityKey),
            };
        }
    };

    template<>
    struct into<SHR::Arrhythmia>
    {
        template<typename TC>
        static basic_value<TC> into_toml(const SHR::Arrhythmia &arrhythmia)
        {
            basic_value<TC> susceptibility(static_cast<double>(arrhythmia.Susceptibility));
            susceptibility.comments().push_back(" Multiplier on PVC frequency and run-extension.");

            basic_value<TC> value(typename basic_value<TC>::table_type{ });
            value[SHR::Arrhythmia::SusceptibilityKey] = susceptibility;
            return value;
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
        template<typename TC>
        static basic_value<TC> into_toml(const SHR::Input &input)
        {
            basic_value<TC> listen(static_cast<std::int64_t>(input.Listen));
            listen.comments().push_back(" Key to toggle notifications and heartbeat audio. 0x23 (35) = H.");

            basic_value<TC> value(typename basic_value<TC>::table_type{ });
            value[SHR::Input::ListenKey] = listen;
            return value;
        }
    };

    template<>
    struct from<SHR::Audio>
    {
        static SHR::Audio from_toml(const toml::value &value)
        {
            return {
                .Volume = toml::find_or<float>(value, SHR::Audio::VolumeKey, SHR::Audio{}.Volume),
            };
        }
    };

    template<>
    struct into<SHR::Audio>
    {
        template<typename TC>
        static basic_value<TC> into_toml(const SHR::Audio &audio)
        {
            basic_value<TC> volume(static_cast<double>(audio.Volume));
            volume.comments().push_back(" Heartbeat output level. The default is already close to the highest level");
            volume.comments().push_back(" without clipping or distortion.");

            basic_value<TC> value(typename basic_value<TC>::table_type{ });
            value[SHR::Audio::VolumeKey] = volume;
            return value;
        }
    };

    template<>
    struct from<SHR::Notification>
    {
        static SHR::Notification from_toml(const toml::value &value)
        {
            return {
                .Enabled    = toml::find<bool>(value, SHR::Notification::EnabledKey),
                .Pulse      = toml::find<std::vector<std::string>>(value, SHR::Notification::PulseKey),
                .Dying      = toml::find<std::string>(value, SHR::Notification::DyingKey),
                .Dead       = toml::find<std::string>(value, SHR::Notification::DeadKey),
                .Arrhythmia = toml::find<std::string>(value, SHR::Notification::ArrhythmiaKey),
            };
        }
    };

    template<>
    struct into<SHR::Notification>
    {
        template<typename TC>
        static basic_value<TC> into_toml(const SHR::Notification &notification)
        {
            basic_value<TC> pulse(notification.Pulse);
            pulse.comments().push_back(" Strings printed in order of increasing HR. All six must be filled to enable.");

            basic_value<TC> value(typename basic_value<TC>::table_type{ });
            value[SHR::Notification::EnabledKey]    = notification.Enabled;
            value[SHR::Notification::PulseKey]      = pulse;
            value[SHR::Notification::DyingKey]      = notification.Dying;
            value[SHR::Notification::DeadKey]       = notification.Dead;
            value[SHR::Notification::ArrhythmiaKey] = notification.Arrhythmia;
            return value;
        }
    };

    template<>
    struct from<SHR::Config>
    {
        static SHR::Config from_toml(const toml::value &value)
        {
            return {
                .Debug        = toml::find<SHR::Debug>(value, SHR::Config::DebugKey),
                .HeartRate    = toml::find<SHR::HeartRate>(value, SHR::Config::HeartRateKey),
                .Arrhythmia   = toml::find<SHR::Arrhythmia>(value, SHR::Config::ArrhythmiaKey),
                .Input        = toml::find<SHR::Input>(value, SHR::Config::InputKey),
                // [audio] is optional: pre-existing configs without it get the defaults.
                .Audio        = value.contains(SHR::Config::AudioKey)
                                    ? toml::find<SHR::Audio>(value, SHR::Config::AudioKey)
                                    : SHR::Audio{ },
                .Notification = toml::find<SHR::Notification>(value, SHR::Config::NotificationKey),
            };
        }
    };

    template<>
    struct into<SHR::Config>
    {
        template<typename TC>
        static basic_value<TC> into_toml(const SHR::Config &config)
        {
            basic_value<TC> value(typename basic_value<TC>::table_type{ });
            value.comments().push_back(" SHR configuration. Delete this file to regenerate defaults.");
            value[SHR::Config::DebugKey]        = basic_value<TC>(config.Debug);
            value[SHR::Config::HeartRateKey]    = basic_value<TC>(config.HeartRate);
            value[SHR::Config::ArrhythmiaKey]   = basic_value<TC>(config.Arrhythmia);
            value[SHR::Config::InputKey]        = basic_value<TC>(config.Input);
            value[SHR::Config::AudioKey]        = basic_value<TC>(config.Audio);
            value[SHR::Config::NotificationKey] = basic_value<TC>(config.Notification);
            return value;
        }
    };
}

const SHR::Config &SHR::Config::Get()
{
    return s_Config;
}

void SHR::Config::Set(Config config)
{
    s_Config = std::move(config);
}

void SHR::Config::Init(std::string_view configPath)
{
    const std::filesystem::path path(configPath);
    if (!std::filesystem::exists(path))
    {
        spdlog::info("Config: '{}' not found - generating defaults.", path.string());
        std::error_code errorCode;
        std::filesystem::create_directories(path.parent_path(), errorCode);
        std::ofstream out(path);
        out << toml::format(toml::ordered_value(Config{ }));
    }

    const auto data = toml::parse(path);
    auto config = toml::from<Config>::from_toml(data);

    const std::size_t pulseCount = config.Notification.Pulse.size();

    if (config.Notification.Enabled)
    {
        if (pulseCount < NotificationPolicy::RequiredPulseCount)
        {
            spdlog::error(
                "Config: pulse notification array requires at least {} entries (got {}) - notifications disabled.",
                NotificationPolicy::RequiredPulseCount,
                pulseCount
            );
            config.Notification.Enabled = false;
        }
        else if (pulseCount > NotificationPolicy::RequiredPulseCount)
        {
            spdlog::warn(
                "Config: pulse notification array has {} entries - only the first {} will be used.",
                pulseCount,
                NotificationPolicy::RequiredPulseCount
            );
        }
    }

    s_Config = std::move(config);
}
