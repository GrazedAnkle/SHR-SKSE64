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
#include "SkyrimHeartRate.hpp"

#include "Config.hpp"
#include "Constants.hpp"
#include "HeartbeatVoice.hpp"
#include "InputHandler.hpp"
#include "NotificationPolicy.hpp"
#include "RhythmEngine.hpp"
#include "Simulation.hpp"

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

    void InitSerialization();

    void OnSave(SKSE::SerializationInterface *serde);
    void OnRevert(SKSE::SerializationInterface *serde);
    void OnLoad(SKSE::SerializationInterface *serde);

    void Update(const RE::PlayerCharacter *player, float delta);
    void HandleFeedback(const RE::PlayerCharacter *player, float delta);

    SHR::PlayerState FromPlayer(const RE::PlayerCharacter *player);

    REL::Relocation<decltype(Update)> s_OriginalUpdate;

    SHR::HeartRateSimulation s_Simulation;
    SHR::RhythmEngine        s_RhythmEngine;
    SHR::HeartbeatVoice      s_HeartbeatVoice;

    float s_LastHoursPassed = 0.0F;

    SHR::HeartRateLevel s_PreviousHeartRateLevel;
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
    s_Simulation.Init();
    s_RhythmEngine.Init();
    s_HeartbeatVoice.Init();
    s_LastHoursPassed = RE::Calendar::GetSingleton()->GetHoursPassed();
}

void SHR::HeartRateManager::NotifyJump()
{
    s_Simulation.NotifyJump();
}

void SHR::HeartRateManager::NotifySleep(float duration)
{
    s_Simulation.NotifySleep(duration);
}

void SHR::HeartRateManager::NotifyFastTravel(float duration)
{
    s_Simulation.NotifyFastTravel(duration);
}

void SHR::HeartRateManager::NotifyCombatEntry()
{
    s_Simulation.NotifyCombatEntry();
}

void SHR::HeartRateManager::NotifyHit()
{
    s_Simulation.NotifyHit();
}

float SHR::HeartRateManager::GetHeartRate()
{
    return s_Simulation.GetHeartRate();
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
        const float heartRate = s_Simulation.GetHeartRate();
        if (!serde->WriteRecord(Record::HeartRate::Type, Record::HeartRate::Version, heartRate))
        {
            SKSE::log::error("Failed to serialize heart rate");
        }

        const float fastHR = s_Simulation.GetFastHR();
        if (!serde->WriteRecord(Record::FastHR::Type, Record::FastHR::Version, fastHR))
        {
            SKSE::log::error("Failed to serialize fast HR component");
        }

        const float exertion = s_Simulation.GetExertion();
        if (!serde->WriteRecord(Record::Exertion::Type, Record::Exertion::Version, exertion))
        {
            SKSE::log::error("Failed to serialize exertion");
        }

        const float adrenaline = s_Simulation.GetAdrenaline();
        if (!serde->WriteRecord(Record::Adrenaline::Type, Record::Adrenaline::Version, adrenaline))
        {
            SKSE::log::error("Failed to serialize adrenaline");
        }

        const float fitness = s_Simulation.GetFitness();
        if (!serde->WriteRecord(Record::Fitness::Type, Record::Fitness::Version, fitness))
        {
            SKSE::log::error("Failed to serialize fitness");
        }

        const float acuteFatigue = s_Simulation.GetAcuteFatigue();
        if (!serde->WriteRecord(Record::AcuteFatigue::Type, Record::AcuteFatigue::Version, acuteFatigue))
        {
            SKSE::log::error("Failed to serialize acute fatigue");
        }

        const float longTermFatigue = s_Simulation.GetLongTermFatigue();
        if (!serde->WriteRecord(Record::LongTermFatigue::Type, Record::LongTermFatigue::Version, longTermFatigue))
        {
            SKSE::log::error("Failed to serialize long-term fatigue");
        }

        const float respRate = s_Simulation.GetRespRate();
        if (!serde->WriteRecord(Record::RespRate::Type, Record::RespRate::Version, respRate))
        {
            SKSE::log::error("Failed to serialize respiratory rate");
        }

        const float contractility = s_Simulation.GetContractility();
        if (!serde->WriteRecord(Record::Contractility::Type, Record::Contractility::Version, contractility))
        {
            SKSE::log::error("Failed to serialize contractility");
        }

        const float respDepth = s_Simulation.GetRespDepth();
        if (!serde->WriteRecord(Record::RespDepth::Type, Record::RespDepth::Version, respDepth))
        {
            SKSE::log::error("Failed to serialize respiratory depth");
        }
    }

    void OnRevert([[maybe_unused]] SKSE::SerializationInterface *serde)
    {
        s_Simulation.Init();
        s_RhythmEngine.Init();
        s_HeartbeatVoice.FlushAndStop();
        s_LastHoursPassed = RE::Calendar::GetSingleton()->GetHoursPassed();
    }

    void OnLoad(SKSE::SerializationInterface *serde)
    {
        std::uint32_t recordType;
        std::uint32_t recordSize;
        std::uint32_t recordVersion;

        const float restingHR = SHR::Config::Get().HeartRate.Resting;
        float heartRate = restingHR;
        float fastHR = 0.0F;
        float exertion = C::IdleMets;
        float adrenaline = 0.0F;
        float fitness = C::FitnessBaseMets + (C::BaseRestingHR - restingHR) / C::RestingHRSlope;
        float acuteFatigue = 0.0F;
        float longTermFatigue = 0.0F;
        float respRate = C::RestingRespRate;
        // Negative sentinels select backward-compatible defaults when older saves omit these records.
        float contractility = -1.0F;
        float respDepth = -1.0F;

        while (serde->GetNextRecordInfo(recordType, recordVersion, recordSize))
        {
            switch (recordType)
            {
            case Record::HeartRate::Type:
                serde->ReadRecordData(heartRate);
                break;
            case Record::FastHR::Type:
                serde->ReadRecordData(fastHR);
                break;
            case Record::Exertion::Type:
                serde->ReadRecordData(exertion);
                break;
            case Record::Adrenaline::Type:
                serde->ReadRecordData(adrenaline);
                break;
            case Record::Fitness::Type:
                serde->ReadRecordData(fitness);
                break;
            case Record::AcuteFatigue::Type:
                serde->ReadRecordData(acuteFatigue);
                break;
            case Record::LongTermFatigue::Type:
                serde->ReadRecordData(longTermFatigue);
                break;
            case Record::RespRate::Type:
                serde->ReadRecordData(respRate);
                break;
            case Record::Contractility::Type:
                serde->ReadRecordData(contractility);
                break;
            case Record::RespDepth::Type:
                serde->ReadRecordData(respDepth);
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

        s_Simulation.Restore(
            heartRate,
            exertion,
            adrenaline,
            fitness,
            acuteFatigue,
            longTermFatigue,
            fastHR,
            respRate,
            contractility,
            respDepth
        );
    }

    void Update(const RE::PlayerCharacter *player, float delta)
    {
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

        const SHR::PlayerState playerState = FromPlayer(player);
        s_Simulation.Step(playerState, delta, gameHoursDelta);
        HandleFeedback(player, delta);
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

    void HandleFeedback(const RE::PlayerCharacter *player, float delta)
    {
        if (!SHR::InputHandler::IsListening())
        {
            return;
        }

        const float heartRate = s_Simulation.GetHeartRate();

        constexpr float maxDeathSeconds = 2.0F * SHR::HeartRateManager::DeathArrhythmiaChanceIncreaseDuration;
        const float deathFactor =
            s_Simulation.GetDeathSeconds()
                .transform([maxDeathSeconds](float seconds) {
                  return std::min(seconds, maxDeathSeconds) / maxDeathSeconds;
                })
                .value_or(0.0F);
        const float hrRange = SHR::Config::Get().HeartRate.Max - SHR::VeryHighHeartRateThreshold;
        const float extremeHRFactor = std::clamp((heartRate - SHR::VeryHighHeartRateThreshold) / hrRange, 0.0F, 1.0F);
        const float fatigueFactor = s_Simulation.GetLongTermFatigue() / C::LongTermFatigueMax;
        const float riskFactor = std::max({ deathFactor, extremeHRFactor, fatigueFactor });

        const float effectiveFitness = s_Simulation.GetEffectiveFitness();
        const float exertionFraction = std::clamp(
            (s_Simulation.GetExertion() - C::IdleMets) / (effectiveFitness - C::IdleMets),
            0.0F,
            1.0F
        );

        const float susceptibility = SHR::Config::Get().Arrhythmia.Susceptibility;

        const float pvcChance = susceptibility * std::lerp(C::PVCChanceNormal, C::PVCChanceMax, riskFactor);

        const float adrenalineFactor   = std::min(s_Simulation.GetAdrenaline() / 5.0F, 1.0F);
        const float acuteFatigueFactor = s_Simulation.GetAcuteFatigue() / C::AcuteFatigueMax;
        const float runExtensionChance = std::min(
            susceptibility *
            C::PVCRunExtensionChance *
            (1.0F + adrenalineFactor + acuteFatigueFactor + extremeHRFactor),
            1.0F
        );

        const auto beat = s_RhythmEngine.Advance(
            delta,
            heartRate,
            s_Simulation.GetRespPhase(),
            exertionFraction,
            s_Simulation.GetRespDepth(),
            s_Simulation.GetContractility(),
            s_Simulation.GetContractilityExcess(),
            pvcChance,
            riskFactor,
            runExtensionChance
        );

        if (!beat.ShouldFire)
        {
            return;
        }

        if (beat.IsPVC)
        {
            const auto notification = SHR::NotificationPolicy::SelectArrhythmia(
                SHR::Config::Get().Notification
            );
            if (notification)
            {
                RE::SendHUDMessage::ShowHUDMessage(notification->data());
            }
        }

        s_HeartbeatVoice.Play(beat);

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
