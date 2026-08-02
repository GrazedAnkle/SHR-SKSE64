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

#include "adapter/CoSave.hpp"
#include "adapter/Config.hpp"
#include "adapter/NotificationPolicy.hpp"
#include "core/Runtime.hpp"
#include "plugin/PluginState.hpp"

namespace
{
    constexpr std::uint32_t CoSaveId = std::byteswap('SHRS');

    void InitSerialization();

    void OnSave(SKSE::SerializationInterface *serde);
    void OnRevert(SKSE::SerializationInterface *serde);
    void OnLoad(SKSE::SerializationInterface *serde);
    void ReportRecord(const char *name, SHR::CoSave::RecordFamily family, SHR::CoSave::RecordVerdict verdict);

    void Update(const RE::PlayerCharacter *player, float delta);
    void HandleFeedback(const RE::PlayerCharacter *player, const SHR::StepResult &result);

    SHR::PlayerState FromPlayer(const RE::PlayerCharacter *player);

    REL::Relocation<decltype(Update)> s_OriginalUpdate;

    SHR::Runtime &RuntimeInstance()
    {
        return SHR::PluginState::Get().GetRuntime();
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
    PluginState::Get().Init(Config::Get(), RE::Calendar::GetSingleton()->GetHoursPassed());
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
    // InputHandler calls this from the engine's worker pool, so it cannot read live state.
    return RuntimeInstance().GetPublishedHeartRate();
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
        const SHR::SimulationState state = RuntimeInstance().GetState();

        for (const SHR::CoSave::RecordValue &record : SHR::CoSave::RecordsToWrite(state))
        {
            if (!serde->WriteRecord(record.Type, record.Version, record.Value))
            {
                SKSE::log::error(FMT_STRING("Failed to serialize co-save record {}"), record.Name);
            }
        }
    }

    void OnRevert([[maybe_unused]] SKSE::SerializationInterface *serde)
    {
        SHR::PluginState::Get().Revert(RE::Calendar::GetSingleton()->GetHoursPassed());
    }

    // Only a record that arrived and could not be used is reported; absence is silent by design.
    void ReportRecord(
        const char                 *name,
        SHR::CoSave::RecordFamily   family,
        SHR::CoSave::RecordVerdict  verdict
    )
    {
        if (verdict == SHR::CoSave::RecordVerdict::Accepted)
        {
            return;
        }
        SKSE::log::warn(
            FMT_STRING("Co-save record {}: {}; {}."),
            name,
            SHR::CoSave::Describe(verdict),
            SHR::CoSave::Describe(SHR::CoSave::ConsequenceOf(family, verdict))
        );
    }

    void OnLoad(SKSE::SerializationInterface *serde)
    {
        std::uint32_t recordType;
        std::uint32_t recordSize;
        std::uint32_t recordVersion;

        SHR::CoSave::CoSaveRecords records;

        while (serde->GetNextRecordInfo(recordType, recordVersion, recordSize))
        {
            const SHR::CoSave::RecordSpec *spec = SHR::CoSave::FindRecord(recordType);
            if (spec == nullptr)
            {
                // GetNextRecordInfo seeks past whatever the previous record left unread, so an
                // unread record needs no skip of its own.
                const std::uint32_t type = std::byteswap(recordType);
                const char *typeBytes = reinterpret_cast<const char *>(&type);
                SKSE::log::warn(
                    FMT_STRING("Encountered unknown record type in co-save: {:.{}}"),
                    typeBytes,
                    sizeof(type)
                );
                continue;
            }

            // Read only after the header clears, so a malformed record cannot leave a partially
            // overwritten float behind.
            const SHR::CoSave::RecordVerdict header = SHR::CoSave::ClassifyHeader(
                spec,
                recordVersion,
                recordSize
            );
            if (header != SHR::CoSave::RecordVerdict::Accepted)
            {
                ReportRecord(spec->Name, spec->Family, header);
                continue;
            }

            float value = 0.0F;
            const std::uint32_t bytesRead = serde->ReadRecordData(value);
            ReportRecord(
                spec->Name,
                spec->Family,
                records.Accept(recordType, recordVersion, recordSize, bytesRead, value)
            );
        }

        SHR::Runtime &runtime = RuntimeInstance();
        runtime.Restore(
            SHR::CoSave::RestoreSimulationState(
                records,
                runtime.CreateInitialState(),
                runtime.GetState().DeathSeconds,
                [&runtime](const SHR::SimulationState &state) {
                    return runtime.ComputeEquilibriumContractility(state);
                }
            )
        );
    }

    void Update(const RE::PlayerCharacter *player, float delta)
    {
        s_OriginalUpdate(player, delta);

        SHR::PluginState &state = SHR::PluginState::Get();
        if (delta == 0.0F)
        {
            state.GetVoice().Pause();
            return;
        }

        state.GetVoice().Resume();

        const float gameHoursDelta = state.ConsumeGameHoursDelta(
            RE::Calendar::GetSingleton()->GetHoursPassed()
        );

        const SHR::StepResult result = state.GetRuntime().Step({
            .Player         = FromPlayer(player),
            .DeltaSeconds   = delta,
            .GameHoursDelta = gameHoursDelta,
            .OutputEnabled  = state.IsListening(),
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

        SHR::PluginState &state = SHR::PluginState::Get();
        state.GetVoice().Play(result.Beat->Render);

        const float heartRate = result.Physiology.HeartRate;
        if (state.GetLevelTracker().Observe(heartRate))
        {
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
