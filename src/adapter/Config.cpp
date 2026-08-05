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

#include <toml.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>

namespace
{
    // Never null, so a reader arriving before Init sees defaults rather than dereferencing nothing.
    std::atomic<std::shared_ptr<const SHR::Config>> s_Config{ std::make_shared<const SHR::Config>() };

    // Remembered by Init so a later write knows where the file is. Empty until then, which is what
    // makes Persist a no-op in tests that never named one.
    std::filesystem::path s_ConfigPath;

    std::string LevelName(spdlog::level::level_enum level)
    {
        const auto view = spdlog::level::to_string_view(level);
        return std::string(view.data(), view.size());
    }

    // A bare string is the one-message pool.
    SHR::MessagePool AsPool(const toml::value &value)
    {
        if (value.is_string())
        {
            return { toml::get<std::string>(value) };
        }

        return toml::get<SHR::MessagePool>(value);
    }

    // Written back in the simplest form that expresses what is configured.
    template<typename TC>
    toml::basic_value<TC> PoolValue(const SHR::MessagePool &pool)
    {
        if (pool.size() == 1)
        {
            return toml::basic_value<TC>(pool.front());
        }

        return toml::basic_value<TC>(pool);
    }

    // Sets one key in a parsed document without disturbing the rest. The section is created when a
    // file predates the key.
    void Assign(
        toml::ordered_value &document,
        const char          *section,
        const char          *key,
        toml::ordered_value  value
    )
    {
        toml::ordered_value &table = document[section];
        if (!table.is_table())
        {
            table = toml::ordered_value(toml::ordered_table{ });
        }

        // A comment belongs to the value it sits above, and assignment replaces the whole value.
        toml::ordered_value &slot = table[key];
        const auto comments = slot.comments();
        slot = std::move(value);
        slot.comments() = comments;
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
                // Optional: configs written before the ceiling existed keep the default.
                .FitnessCeiling = toml::find_or<float>(
                    value,
                    SHR::HeartRate::FitnessCeilingKey,
                    SHR::HeartRate{ }.FitnessCeiling
                ),
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
            basic_value<TC> ceiling(static_cast<double>(heartRate.FitnessCeiling));
            ceiling.comments().push_back(" Aerobic capacity ceiling in mL/kg/min; what training converges toward.");

            value[SHR::HeartRate::RestingKey]        = resting;
            value[SHR::HeartRate::MaxKey]            = static_cast<double>(heartRate.Max);
            value[SHR::HeartRate::FitnessCeilingKey] = ceiling;
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
            const toml::value &pulse = toml::find(value, SHR::Notification::PulseKey);

            std::vector<SHR::MessagePool> pools;
            pools.reserve(pulse.size());
            for (const toml::value &band : pulse.as_array())
            {
                pools.push_back(AsPool(band));
            }

            return {
                .Enabled    = toml::find<bool>(value, SHR::Notification::EnabledKey),
                .Pulse      = std::move(pools),
                .Dying      = AsPool(toml::find(value, SHR::Notification::DyingKey)),
                .Dead       = AsPool(toml::find(value, SHR::Notification::DeadKey)),
                .Arrhythmia = AsPool(toml::find(value, SHR::Notification::ArrhythmiaKey)),
            };
        }
    };

    template<>
    struct into<SHR::Notification>
    {
        template<typename TC>
        static basic_value<TC> into_toml(const SHR::Notification &notification)
        {
            basic_value<TC> enabled(notification.Enabled);
            enabled.comments().push_back(" Notification messages about your character's heart. Toggling can be done from");
            enabled.comments().push_back(" the mod menu as well.");

            typename basic_value<TC>::array_type bands;
            bands.reserve(notification.Pulse.size());
            for (const SHR::MessagePool &band : notification.Pulse)
            {
                bands.push_back(PoolValue<TC>(band));
            }

            basic_value<TC> pulse(std::move(bands));
            pulse.comments().push_back(" One message per heart-rate range, from lowest to highest: resting, idle,");
            pulse.comments().push_back(" elevated, high, very high, extreme. Shown when heart rate crosses into that");
            pulse.comments().push_back(" range. All six must be present or notifications will stay off.");
            pulse.comments().push_back(" ");
            pulse.comments().push_back(" Anywhere one message is expected, you can also provide a list of");
            pulse.comments().push_back(" interchangeable wordings. One of them will be drawn each time that message is");
            pulse.comments().push_back(" shown:");
            pulse.comments().push_back(" ");
            pulse.comments().push_back("   pulse = [");
            pulse.comments().push_back(R"(     "This message will show up verbatim at resting HR.",)");
            pulse.comments().push_back(R"(     ["At idle HR, this message will show up.", "Or this one."],)");
            pulse.comments().push_back("     # ... four more, one per band.");
            pulse.comments().push_back("   ]");
            pulse.comments().push_back(" ");
            pulse.comments().push_back(" dying, dead and arrhythmia accept both forms as well.");

            basic_value<TC> dying = PoolValue<TC>(notification.Dying);
            dying.comments().push_back(" Shown while your character is bleeding out.");

            basic_value<TC> dead = PoolValue<TC>(notification.Dead);
            dead.comments().push_back(" Shown once your character's heart has stopped.");

            basic_value<TC> arrhythmia = PoolValue<TC>(notification.Arrhythmia);
            arrhythmia.comments().push_back(" Shown when a beat skips. This one fires the most often, so it is the one");
            arrhythmia.comments().push_back(" that would benefit the most from several wordings.");

            basic_value<TC> value(typename basic_value<TC>::table_type{ });
            value[SHR::Notification::EnabledKey]    = enabled;
            value[SHR::Notification::PulseKey]      = pulse;
            value[SHR::Notification::DyingKey]      = dying;
            value[SHR::Notification::DeadKey]       = dead;
            value[SHR::Notification::ArrhythmiaKey] = arrhythmia;
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

std::shared_ptr<const SHR::Config> SHR::Config::Get()
{
    return s_Config.load(std::memory_order_acquire);
}

void SHR::Config::Set(Config config)
{
    // Publishes rather than mutates: a mutating write would free Notification's strings under a
    // reader on the worker pool.
    s_Config.store(std::make_shared<const Config>(std::move(config)), std::memory_order_release);
}

bool SHR::Config::Persist()
{
    if (s_ConfigPath.empty())
    {
        return false;
    }

    const std::shared_ptr<const Config> config = Get();
    try
    {
        // Re-parsed rather than reserialized: the file is hand-editable and only these keys are ours.
        toml::ordered_value document = toml::parse<toml::ordered_type_config>(s_ConfigPath);

        Assign(document, AudioKey, SHR::Audio::VolumeKey, toml::ordered_value(config->Audio.Volume));
        Assign(document, InputKey, SHR::Input::ListenKey, toml::ordered_value(config->Input.Listen));
        Assign(
            document,
            NotificationKey,
            SHR::Notification::EnabledKey,
            toml::ordered_value(config->Notification.Enabled)
        );

        std::ofstream out(s_ConfigPath);
        out << toml::format(document);
        return out.good();
    }
    catch (const std::exception &error)
    {
        // The value stays applied in memory; only the next launch loses it.
        spdlog::error("Config: could not write '{}' - {}", s_ConfigPath.string(), error.what());
        return false;
    }
}

void SHR::Config::Init(std::string_view configPath)
{
    const std::filesystem::path path(configPath);
    s_ConfigPath = path;
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
        // An empty pool has no message to draw, so it fails the same way a missing band does.
        else if (const auto empty = NotificationPolicy::FindEmptyBand(config.Notification))
        {
            spdlog::error(
                "Config: pulse notification band {} has no message - notifications disabled.",
                *empty
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

    Set(std::move(config));
}
