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
    RE::BGSSoundDescriptorForm *HeartBeat = nullptr;
    RE::BGSSoundDescriptorForm *HeartBeatElevated = nullptr;
    RE::BGSSoundDescriptorForm *HeartBeatHigh = nullptr;
    RE::BGSSoundDescriptorForm *HeartBeatVeryHigh = nullptr;

    RE::BGSSoundDescriptorForm *LoadSoundDescriptorForm(RE::TESDataHandler *data, RE::FormID formId)
    {
        auto *sound = data->LookupForm<RE::BGSSoundDescriptorForm>(formId, "SkyrimHeartRate.esp");

        if (!sound)
        {
            const std::string message = fmt::format(FMT_STRING("Failed to load sound {:#08d}!"), formId);
            SKSE::stl::report_and_fail(message);
        }

        return sound;
    }
}

void SHR::Sound::Init()
{
    auto *data = RE::TESDataHandler::GetSingleton();

    if (!data)
    {
        SKSE::stl::report_and_fail("Failed to retrieve TESDataHandler!");
    }

    HeartBeat         = LoadSoundDescriptorForm(data, 0x1832);
    HeartBeatElevated = LoadSoundDescriptorForm(data, 0x1834);
    HeartBeatHigh     = LoadSoundDescriptorForm(data, 0x1DA6);
    HeartBeatVeryHigh = LoadSoundDescriptorForm(data, 0x5E76);
}

RE::BGSSoundDescriptorForm *SHR::Sound::GetDescriptorForm(float heartRate)
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

bool Sound::Play(RE::BGSSoundDescriptorForm *form, const RE::Actor *actor)
{
    RE::BSSoundHandle sound;
    sound.soundID = RE::BSSoundHandle::kInvalidID;
    sound.assumeSuccess = false;
    sound.state = RE::BSSoundHandle::AssumedState::kInitialized;

    auto *manager = RE::BSAudioManager::GetSingleton();
    if (!manager->BuildSoundDataFromDescriptor(sound, form->soundDescriptor))
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
