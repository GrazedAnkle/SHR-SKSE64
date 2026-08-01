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
#include "plugin/SkyrimHeartRate.hpp"

#include "adapter/Config.hpp"
#include "core/Constants.hpp"
#include "plugin/HeartbeatVoice.hpp"
#include "plugin/InputHandler.hpp"
#include "adapter/NotificationPolicy.hpp"
#include "core/Runtime.hpp"
#include "plugin/ThreadTrace.hpp"

#include <optional>

namespace
{
    namespace C = SHR::Constants;

    constexpr std::uint32_t CoSaveId = std::byteswap('SHRS');

    namespace Record
    {
        struct HeartRate
        {
            static constexpr std::uint32_t Type = std::byteswap('PCHR');
            static constexpr std::uint32_t Version = 0;
        };

        struct Exertion
        {
            static constexpr std::uint32_t Type = std::byteswap('EXRT');
            static constexpr std::uint32_t Version = 0;
        };

        struct Adrenaline
        {
            static constexpr std::uint32_t Type = std::byteswap('ADRL');
            static constexpr std::uint32_t Version = 0;
        };

        struct Fitness
        {
            static constexpr std::uint32_t Type = std::byteswap('FTNS');
            static constexpr std::uint32_t Version = 0;
        };

        struct AcuteFatigue
        {
            static constexpr std::uint32_t Type = std::byteswap('AFTG');
            static constexpr std::uint32_t Version = 0;
        };

        struct LongTermFatigue
        {
            static constexpr std::uint32_t Type = std::byteswap('LFTG');
            static constexpr std::uint32_t Version = 0;
        };

        struct FastHR
        {
            static constexpr std::uint32_t Type = std::byteswap('FAHR');
            static constexpr std::uint32_t Version = 0;
        };

        struct RespRate
        {
            static constexpr std::uint32_t Type = std::byteswap('RRTE');
            static constexpr std::uint32_t Version = 0;
        };

        struct Contractility
        {
            static constexpr std::uint32_t Type = std::byteswap('CTLY');
            static constexpr std::uint32_t Version = 0;
        };

        struct RespDepth
        {
            static constexpr std::uint32_t Type = std::byteswap('RDPT');
            static constexpr std::uint32_t Version = 0;
        };
    }

    struct LegacyCoSaveState
    {
        std::optional<float> HeartRate;
        std::optional<float> FastHeartRate;
        std::optional<float> Exertion;
        std::optional<float> Adrenaline;
        std::optional<float> Fitness;
        std::optional<float> AcuteFatigue;
        std::optional<float> LongTermFatigue;
        std::optional<float> RespirationRate;
        std::optional<float> Contractility;
        std::optional<float> RespirationDepth;
    };

    void InitSerialization();

    void OnSave(SKSE::SerializationInterface *serde);
    void OnRevert(SKSE::SerializationInterface *serde);
    void OnLoad(SKSE::SerializationInterface *serde);
    SHR::SimulationState FromLegacyCoSave(const LegacyCoSaveState &records);

    void Update(const RE::PlayerCharacter *player, float delta);
    void HandleFeedback(const RE::PlayerCharacter *player, const SHR::StepResult &result);

    SHR::PlayerState FromPlayer(const RE::PlayerCharacter *player);

    REL::Relocation<decltype(Update)> s_OriginalUpdate;

    std::optional<SHR::Runtime> s_Runtime;

    // Deliberately never destroyed: DestroyVoice waits on the XAudio2 audio thread, and at process
    // exit the BSXAudio2Audio engine owning the voice may already be gone. Revisited by WI-026.
    SHR::HeartbeatVoice &s_HeartbeatVoice = *new SHR::HeartbeatVoice();

    float s_LastHoursPassed = 0.0F;

    SHR::HeartRateLevel s_PreviousHeartRateLevel;

    SHR::Runtime &RuntimeInstance()
    {
        return s_Runtime.value();
    }
}

void SHR::InstallHooks()
{
    SKSE::log::trace("Initializing trampoline...");
    SKSE::Trampoline &trampoline = SKSE::GetTrampoline();
    trampoline.create(64);
    SKSE::log::trace("Trampoline initialized.");

    SKSE::log::trace("Installing hooks...");
    HeartRateManager::InstallHooks(trampoline);
    SKSE::log::trace("Hooks installed.");
}

void SHR::HeartRateManager::InstallHooks(SKSE::Trampoline &trampoline)
{
    s_OriginalUpdate = REL::Relocation<std::uintptr_t>(RE::VTABLE_PlayerCharacter[0])
        .write_vfunc(0xAD, Update);
}

void SHR::HeartRateManager::Init()
{
    InitSerialization();
    const auto &config = Config::Get();
    s_Runtime.emplace(RuntimeSettings{
        .Simulation = {
            .RestingHeartRate = config.HeartRate.Resting,
            .MaximumHeartRate = config.HeartRate.Max,
        },
        .ArrhythmiaSusceptibility = config.Arrhythmia.Susceptibility,
    });
    RuntimeInstance().Init();
    s_HeartbeatVoice.Init(config.Audio.Volume);
    s_LastHoursPassed = RE::Calendar::GetSingleton()->GetHoursPassed();
}

void SHR::HeartRateManager::NotifyJump()
{
    RuntimeInstance().NotifyJump();
}

void SHR::HeartRateManager::NotifySleep(float duration)
{
    RuntimeInstance().NotifySleep(duration);
}

void SHR::HeartRateManager::NotifyFastTravel(float duration)
{
    RuntimeInstance().NotifyFastTravel(duration);
}

void SHR::HeartRateManager::NotifyCombatEntry()
{
    RuntimeInstance().NotifyCombatEntry();
}

void SHR::HeartRateManager::NotifyHit()
{
    RuntimeInstance().NotifyHit();
}

float SHR::HeartRateManager::GetHeartRate()
{
    return RuntimeInstance().GetSnapshot().HeartRate;
}

namespace
{
    void InitSerialization()
    {
        SKSE::log::trace("Initializing SKSE co-save serialization...");
        const SKSE::SerializationInterface *serde = SKSE::GetSerializationInterface();
        serde->SetUniqueID(CoSaveId);
        serde->SetSaveCallback(OnSave);
        serde->SetRevertCallback(OnRevert);
        serde->SetLoadCallback(OnLoad);
        SKSE::log::trace("SKSE co-save serialization initialized.");
    }

    void OnSave(SKSE::SerializationInterface *serde)
    {
        SHR_TRACE_THREAD("serialization.OnSave");

        const SHR::SimulationState state = RuntimeInstance().GetState();

        const float heartRate = state.FastHeartRate + state.SlowHeartRate;
        if (!serde->WriteRecord(Record::HeartRate::Type, Record::HeartRate::Version, heartRate))
        {
            SKSE::log::error("Failed to serialize heart rate");
        }
        if (!serde->WriteRecord(Record::FastHR::Type, Record::FastHR::Version, state.FastHeartRate))
        {
            SKSE::log::error("Failed to serialize fast HR component");
        }
        if (!serde->WriteRecord(Record::Exertion::Type, Record::Exertion::Version, state.Exertion))
        {
            SKSE::log::error("Failed to serialize exertion");
        }
        if (!serde->WriteRecord(Record::Adrenaline::Type, Record::Adrenaline::Version, state.Adrenaline))
        {
            SKSE::log::error("Failed to serialize adrenaline");
        }
        if (!serde->WriteRecord(Record::Fitness::Type, Record::Fitness::Version, state.Fitness))
        {
            SKSE::log::error("Failed to serialize fitness");
        }
        if (!serde->WriteRecord(Record::AcuteFatigue::Type, Record::AcuteFatigue::Version, state.AcuteFatigue))
        {
            SKSE::log::error("Failed to serialize acute fatigue");
        }
        if (!serde->WriteRecord(Record::LongTermFatigue::Type, Record::LongTermFatigue::Version, state.LongTermFatigue))
        {
            SKSE::log::error("Failed to serialize long-term fatigue");
        }
        if (!serde->WriteRecord(Record::RespRate::Type, Record::RespRate::Version, state.RespirationRate))
        {
            SKSE::log::error("Failed to serialize respiratory rate");
        }
        if (!serde->WriteRecord(Record::Contractility::Type, Record::Contractility::Version, state.Contractility))
        {
            SKSE::log::error("Failed to serialize contractility");
        }
        if (!serde->WriteRecord(Record::RespDepth::Type, Record::RespDepth::Version, state.RespirationDepth))
        {
            SKSE::log::error("Failed to serialize respiratory depth");
        }
    }

    void OnRevert([[maybe_unused]] SKSE::SerializationInterface *serde)
    {
        SHR_TRACE_THREAD("serialization.OnRevert");

        RuntimeInstance().Init();
        s_HeartbeatVoice.FlushAndStop();
        s_LastHoursPassed = RE::Calendar::GetSingleton()->GetHoursPassed();
    }

    void OnLoad(SKSE::SerializationInterface *serde)
    {
        SHR_TRACE_THREAD("serialization.OnLoad");

        std::uint32_t recordType;
        std::uint32_t recordSize;
        std::uint32_t recordVersion;

        LegacyCoSaveState records;

        const SHR::SimulationState initial = RuntimeInstance().CreateInitialState();

        const float initialHeartRate = initial.FastHeartRate + initial.SlowHeartRate;
        const auto readRecord = [serde](std::optional<float> &destination, float fallback) {
            float value = fallback;
            serde->ReadRecordData(value);
            destination = value;
        };

        while (serde->GetNextRecordInfo(recordType, recordVersion, recordSize))
        {
            switch (recordType)
            {
            case Record::HeartRate::Type:
                readRecord(records.HeartRate, initialHeartRate);
                break;
            case Record::FastHR::Type:
                readRecord(records.FastHeartRate, 0.0F);
                break;
            case Record::Exertion::Type:
                readRecord(records.Exertion, initial.Exertion);
                break;
            case Record::Adrenaline::Type:
                readRecord(records.Adrenaline, initial.Adrenaline);
                break;
            case Record::Fitness::Type:
                readRecord(records.Fitness, initial.Fitness);
                break;
            case Record::AcuteFatigue::Type:
                readRecord(records.AcuteFatigue, initial.AcuteFatigue);
                break;
            case Record::LongTermFatigue::Type:
                readRecord(records.LongTermFatigue, initial.LongTermFatigue);
                break;
            case Record::RespRate::Type:
                readRecord(records.RespirationRate, initial.RespirationRate);
                break;
            case Record::Contractility::Type:
                readRecord(records.Contractility, -1.0F);
                break;
            case Record::RespDepth::Type:
                readRecord(records.RespirationDepth, -1.0F);
                break;
            default:
                {
                    const std::uint32_t type = std::byteswap(recordType);
                    const char *typeBytes = reinterpret_cast<const char *>(&type);
                    SKSE::log::warn(FMT_STRING("Encountered unknown record type in co-save: {:.{}}"), typeBytes, sizeof(type));
                    break;
                }
            }
        }

        RuntimeInstance().Restore(FromLegacyCoSave(records));
    }

    SHR::SimulationState FromLegacyCoSave(const LegacyCoSaveState &records)
    {
        SHR::SimulationState state = RuntimeInstance().CreateInitialState();

        const float heartRate = records.HeartRate.value_or(
            state.FastHeartRate + state.SlowHeartRate
        );
        const float fastHeartRate = records.FastHeartRate.value_or(0.0F);
        state.FastHeartRate = fastHeartRate > 0.0F
            ? fastHeartRate
            : C::HRFastFraction * heartRate;
        state.SlowHeartRate = heartRate - state.FastHeartRate;
        state.Exertion = records.Exertion.value_or(state.Exertion);
        state.Adrenaline = records.Adrenaline.value_or(state.Adrenaline);
        state.Fitness = records.Fitness.value_or(state.Fitness);
        if (state.Fitness <= 0.0F)
        {
            state.Fitness = RuntimeInstance().CreateInitialState().Fitness;
        }
        state.AcuteFatigue = records.AcuteFatigue.value_or(state.AcuteFatigue);
        state.LongTermFatigue = records.LongTermFatigue.value_or(state.LongTermFatigue);
        state.RespirationRate = records.RespirationRate.value_or(state.RespirationRate);
        if (state.RespirationRate <= 0.0F)
        {
            state.RespirationRate = C::RestingRespRate;
        }

        const float respirationDepth = records.RespirationDepth.value_or(-1.0F);
        state.RespirationDepth = respirationDepth >= 0.0F
            ? std::clamp(respirationDepth, 0.0F, 1.0F)
            : 0.0F;

        const float contractility = records.Contractility.value_or(-1.0F);
        state.Contractility = contractility >= 0.0F
            ? contractility
            : RuntimeInstance().ComputeEquilibriumContractility(state);

        // The current co-save schema does not persist these fields. Preserve the legacy restore
        // behavior: restart the respiratory oscillator and leave an active death timer untouched.
        state.RespirationPhase = 0.0F;
        state.DeathSeconds = RuntimeInstance().GetState().DeathSeconds;
        return state;
    }

    void Update(const RE::PlayerCharacter *player, float delta)
    {
        // The reference row: every other site is interesting only relative to this one.
        SHR_TRACE_THREAD("hook.PlayerCharacter::Update");

        s_OriginalUpdate(player, delta);

        if (delta == 0.0F)
        {
            s_HeartbeatVoice.Pause();
            return;
        }

        s_HeartbeatVoice.Resume();

        const float currentHours = RE::Calendar::GetSingleton()->GetHoursPassed();
        const float gameHoursDelta = currentHours - s_LastHoursPassed;
        s_LastHoursPassed = currentHours;

        const SHR::StepResult result = RuntimeInstance().Step({
            .Player         = FromPlayer(player),
            .DeltaSeconds   = delta,
            .GameHoursDelta = gameHoursDelta,
            .OutputEnabled  = SHR::InputHandler::IsListening(),
        });
        HandleFeedback(player, result);
    }

    SHR::PlayerState FromPlayer(const RE::PlayerCharacter *player)
    {
        return {
            .IsDead      = player->IsDead(),
            .IsSprinting = player->IsSprinting(),
            .IsRunning   = player->IsRunning(),
            .IsWalking   = player->IsWalking(),
            .IsSwimming  = player->IsSwimming(),
            .IsSneaking  = player->IsSneaking(),
            .IsOnMount   = player->IsOnMount(),
        };
    }

    void HandleFeedback(const RE::PlayerCharacter *player, const SHR::StepResult &result)
    {
        if (!result.Beat)
        {
            return;
        }

        if (result.Beat->Event.Kind == SHR::BeatKind::PVC)
        {
            const auto notification = SHR::NotificationPolicy::SelectArrhythmia(
                SHR::Config::Get().Notification
            );
            if (notification)
            {
                RE::SendHUDMessage::ShowHUDMessage(notification->data());
            }
        }

        s_HeartbeatVoice.Play(result.Beat->Render);

        const float heartRate = result.Physiology.HeartRate;
        const SHR::HeartRateLevel currentLevel = SHR::GetHeartRateLevel(heartRate);
        if (currentLevel != s_PreviousHeartRateLevel)
        {
            s_PreviousHeartRateLevel = currentLevel;
            const auto notification = SHR::NotificationPolicy::SelectStatus(
                SHR::Config::Get().Notification,
                player->IsDead(),
                heartRate
            );
            if (notification)
            {
                RE::SendHUDMessage::ShowHUDMessage(notification->data());
            }
        }
    }
}
