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
#include "Sound.hpp"

namespace
{
    const RE::BGSSoundDescriptorForm *HeartBeat = nullptr;
    const RE::BGSSoundDescriptorForm *HeartBeatElevated = nullptr;
    const RE::BGSSoundDescriptorForm *HeartBeatHigh = nullptr;
    const RE::BGSSoundDescriptorForm *HeartBeatVeryHigh = nullptr;

    const RE::BGSSoundDescriptorForm *LoadSound(RE::TESDataHandler *data, RE::FormID formId)
    {
        auto *sound = data->LookupForm<RE::BGSSoundDescriptorForm>(formId, "SkyrimHeartRate.esp");

        if (!sound)
        {
            const std::string message = fmt::format(FMT_STRING("Failed to load sound {:#08d}!"), formId);
            SKSE::stl::report_and_fail(message);
        }

        return sound;
    }

    // Adapted from D7ry's Valhalla Combat, licensed under MIT. Originally named soundHelper_a.
    bool BuildSoundDataFromFormID(
        RE::BSAudioManager *manager,
        RE::BSSoundHandle &handle,
        RE::FormID formId,
        std::uint32_t flags
    )
    {
        using FunctionSignature = int (*)(RE::BSAudioManager *, RE::BSSoundHandle *, RE::FormID, std::uint32_t);

        // Offset SE: 140BEEE70, AE: 140C13DA0
        static REL::Relocation<FunctionSignature> buildSoundDataFromFormId(RELOCATION_ID(66401, 67663));

        return buildSoundDataFromFormId(manager, &handle, formId, flags) != 0;
    }
}

void SHR::Sound::Init()
{
    auto *data = RE::TESDataHandler::GetSingleton();

    if (!data)
    {
        SKSE::stl::report_and_fail("Failed to retrieve TESDataHandler!");
    }

    HeartBeat         = LoadSound(data, 0x1832);
    HeartBeatElevated = LoadSound(data, 0x1834);
    HeartBeatHigh     = LoadSound(data, 0x1DA6);
    HeartBeatVeryHigh = LoadSound(data, 0x5E76);
}

const RE::BGSSoundDescriptorForm *SHR::Sound::GetDescriptor(float heartRate)
{
    if (heartRate > 180.0F)
    {
        return HeartBeatVeryHigh;
    }
    else if (heartRate > 150.0F)
    {
        return HeartBeatHigh;
    }
    else if (heartRate > 100.0F)
    {
        return HeartBeatElevated;
    }
    else
    {
        return HeartBeat;
    }
}

bool Sound::Play(const RE::BGSSoundDescriptorForm *descriptor, const RE::Actor *actor)
{
    RE::BSSoundHandle sound;
    sound.soundID = RE::BSSoundHandle::kInvalidID;
    sound.assumeSuccess = false;
    sound.state = RE::BSSoundHandle::AssumedState::kInitialized;

    auto *manager = RE::BSAudioManager::GetSingleton();
    if (!BuildSoundDataFromFormID(manager, sound, descriptor->GetFormID(), 16))
    {
        return false;
    }

    if (!sound.SetPosition(actor->GetPosition()))
    {
        return false;
    }

    sound.SetObjectToFollow(actor->Get3D());
    return sound.Play();
}
