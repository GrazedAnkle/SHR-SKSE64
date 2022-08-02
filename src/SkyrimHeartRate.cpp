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

namespace
{
    constexpr std::uint32_t CoSaveId = std::byteswap('SHRS');

    namespace Record
    {
        struct HeartRate
        {
            static constexpr std::uint32_t Type = std::byteswap('PCHR');
            static constexpr std::uint32_t Version = 0;
        };

        struct LongTermStamina
        {
            static constexpr std::uint32_t Type = std::byteswap('LTSX');
            static constexpr std::uint32_t Version = 0;
        };
    }

    void InitSerialization();

    void OnSave(SKSE::SerializationInterface *serde);

    void OnRevert(SKSE::SerializationInterface *serde);

    void OnLoad(SKSE::SerializationInterface *serde);

    void Update(RE::PlayerCharacter *player, float delta);

    void UpdateTargetHeartRate(const RE::PlayerCharacter *player);

    void UpdateCurrentHeartRate(const RE::PlayerCharacter *player, float delta);

    REL::Relocation<decltype(Update)> s_OriginalUpdate;

    constexpr float HeartRateEqualEpsilon = 0.5F;
    float s_TargetHeartRate = 0.0F;
    float s_HeartRate = 60.0F;

    std::atomic_bool s_DidJump;

    std::atomic<float> s_SleepDuration = Float::Sentinel;

    std::atomic<float> s_FastTravelDuration = Float::Sentinel;

    bool s_DidStartDeathTimer = false;
    SHR::HeartRateManager::Timestamp s_DeathTimestamp;

    bool s_DidApplyAdrenaline = false;
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

    s_HeartRate = Config::Get().Limit.Resting;
    s_TargetHeartRate = s_HeartRate;
}

void SHR::HeartRateManager::NotifyJump()
{
    s_DidJump = true;
}

void SHR::HeartRateManager::NotifySleep(float duration)
{
    s_SleepDuration = duration;
}

void SHR::HeartRateManager::NotifyFastTravel(float duration)
{
    s_FastTravelDuration = duration;
}

float SHR::HeartRateManager::GetHeartRate()
{
    return s_HeartRate;
}

std::optional<SHR::HeartRateManager::Timestamp> SHR::HeartRateManager::GetDeathTimestamp()
{
    return s_DidStartDeathTimer ? std::optional(s_DeathTimestamp) : std::nullopt;
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
        const float heartRate = s_HeartRate;
        if (!serde->WriteRecord(Record::HeartRate::Type, Record::HeartRate::Version, heartRate))
        {
            SKSE::log::error("Failed to serialize heart rate");
        }
    }

    void OnRevert([[maybe_unused]] SKSE::SerializationInterface *serde)
    {
        s_HeartRate = SHR::Config::Get().Limit.Resting;
    }

    void OnLoad(SKSE::SerializationInterface *serde)
    {
        std::uint32_t recordType;
        std::uint32_t recordSize;
        std::uint32_t recordVersion;

        while (serde->GetNextRecordInfo(recordType, recordSize, recordVersion))
        {
            switch (recordType)
            {
            case Record::HeartRate::Type:
            {
                float heartRate;
                serde->ReadRecordData(heartRate);
                s_HeartRate = heartRate;
            }
                break;
            case Record::LongTermStamina::Type:
                SKSE::log::warn("Encountered LTSX record which is not yet implemented");
                break;
            default:
                const std::uint32_t type = std::byteswap(recordType);
                const char *typeBytes = reinterpret_cast<const char *>(&type);
                SKSE::log::warn(FMT_STRING("Encountered unknown record type in co-save: {:.{}}"), typeBytes, sizeof(type));
                break;
            }
        }
    }

    void Update(RE::PlayerCharacter *player, float delta)
    {
        s_OriginalUpdate(player, delta);

        if (player)
        {
            UpdateTargetHeartRate(player);
            UpdateCurrentHeartRate(player, delta);
        }
    }

    void UpdateTargetHeartRate(const RE::PlayerCharacter *player)
    {
        if (player->IsDead())
        {
            if (!s_DidStartDeathTimer)
            {
                s_DidStartDeathTimer = true;
                s_DeathTimestamp = std::chrono::steady_clock::now();
            }

            if (s_TargetHeartRate - s_HeartRate > HeartRateEqualEpsilon)
            {
                s_TargetHeartRate = SHR::HeartRateManager::HeartRateFibrillation;
            }
            else
            {
                s_TargetHeartRate = 0.0F;
            }

            return;
        }

        s_DidStartDeathTimer = false;

        const std::array movementLevelHeartRate = {
            SHR::Config::Get().Limit.Idle,
            SHR::Config::Get().Limit.Walking,
            SHR::Config::Get().Limit.Running,
            SHR::Config::Get().Limit.Sprinting,
        };

        std::int32_t movementLevel = 0;
        if (player->IsWalking())
        {
            movementLevel = 1;
        }
        else if (player->IsRunning())
        {
            movementLevel = 2;
        }
        else if (player->IsSprinting())
        {
            movementLevel = 3;
        }
        else if (s_DidJump.exchange(false))
        {
            movementLevel = 3;
        }

        float movementHeartRateOffset = 0.0F;
        if (movementLevel > 0)
        {
            if (player->IsSneaking())
            {
                movementHeartRateOffset = 10.0F;
            }
            else if (player->IsSwimming())
            {
                movementHeartRateOffset = 20.0F;
            }
        }

        if (player->IsOnMount())
        {
            movementLevel = std::max(movementLevel - 1, 0);
        }

        const float movementHeartRate = movementLevelHeartRate[movementLevel] + movementHeartRateOffset;

        const float targetHeartRate = player->IsInCombat()
            ? std::max(movementHeartRate, SHR::Config::Get().Limit.Combat)
            : movementHeartRate;

        s_TargetHeartRate = targetHeartRate;
    }

    // This is basically just a proportional controller.
    void UpdateCurrentHeartRate(const RE::PlayerCharacter *player, float delta)
    {
        // Comparison against 0 is safe here since 0 is set explicitly.
        if (s_TargetHeartRate == 0.0F && s_HeartRate == 0.0F)
        {
            return;
        }

        const float difference = s_TargetHeartRate - s_HeartRate;

        if (std::abs(difference) < HeartRateEqualEpsilon)
        {
            s_HeartRate = s_TargetHeartRate;
        }

        // Equals comparison is safe here since `s_HeartRate` is explicitly set to `s_TargetHeartRate`.
        if (s_HeartRate == s_TargetHeartRate)
        {
            return;
        }

        const bool isNegative = std::signbit(difference);

        // This is normalized to [0, 1].
        const float incDecMultiplier = isNegative ? (1.0F / SHR::Config::Get().Multiplier.IncDecRatio) : 1.0F;

        // This attempts to target a maximum mod amount per second given a rough upper bound on `difference` of 150.
        constexpr float maxChangePerSecond = 4.0F;
        constexpr float attenuationFactor = maxChangePerSecond / 150.0F;
        const float modMultiplier = attenuationFactor * SHR::Config::Get().Multiplier.Mod;

        // Cap to `maxChangePerSecond` to handle unusually large `difference` values.
        const float modAmountPerSecond = std::clamp(
            modMultiplier * incDecMultiplier * difference,
            -maxChangePerSecond,
            maxChangePerSecond
        );

        const float modAmount = delta * modAmountPerSecond;

        s_HeartRate += modAmount;

        constexpr float adrenalineAmount = 10.0F;
        if (player->IsInCombat())
        {
            if (!s_DidApplyAdrenaline)
            {
                s_DidApplyAdrenaline = true;
                if (s_HeartRate + adrenalineAmount < SHR::Config::Get().Limit.Combat)
                {
                    s_HeartRate += adrenalineAmount;
                }
            }
        }
        else
        {
            s_DidApplyAdrenaline = false;
        }

        if (const float duration = s_SleepDuration.exchange(Float::Sentinel); duration != Float::Sentinel)
        {
            s_HeartRate = SHR::Config::Get().Limit.Resting;
        }
    }
}
