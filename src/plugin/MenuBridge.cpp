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
#include "plugin/MenuBridge.hpp"

#include "adapter/Settings.hpp"
#include "plugin/PluginState.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
    using SHR::Settings::Kind;

    // The script name the VM resolves; must match SHR_Native.psc and config.json's action script.
    constexpr auto ScriptName = "SHR_Native";

    constexpr std::string_view PluginFile   = "SHR.esp";
    constexpr RE::FormID       QuestLocalID = 0x800;

    // An exception unwinding into the Papyrus VM is undefined behaviour, so every native is a
    // catch-all boundary.
    template <typename Result, typename Fn>
    Result Guard(const char *what, Result fallback, const Fn &body) noexcept
    {
        try
        {
            return body();
        }
        catch (const std::exception &error)
        {
            SKSE::log::error(FMT_STRING("Menu native {:s} failed: {:s}"), what, error.what());
        }
        catch (...)
        {
            SKSE::log::error(FMT_STRING("Menu native {:s} failed"), what);
        }
        return fallback;
    }

    std::string_view AsView(const RE::BSFixedString &settingId)
    {
        const char *text = settingId.c_str();
        return text != nullptr ? std::string_view{ text } : std::string_view{ };
    }

    // Every setting crosses as a float; the Papyrus type only picks which native the layout calls.
    bool Write(const RE::BSFixedString &settingId, Kind kind, float value)
    {
        const std::string_view name = AsView(settingId);
        SHR::PluginState &state = SHR::PluginState::Get();

        // The two tables differ only in where the value lands, so the checks around it are shared.
        const auto commit = [&](Kind declared, auto &&apply) {
            if (declared != kind)
            {
                SKSE::log::error(FMT_STRING("Menu setting {:s} was written as the wrong type"), name);
                return false;
            }
            if (!apply())
            {
                SKSE::log::warn(FMT_STRING("Menu setting {:s} rejected value {:.3f}"), name, value);
                return false;
            }
            return true;
        };

        if (const SHR::Settings::SubjectSpec *spec = SHR::Settings::FindSubject(name))
        {
            return commit(spec->Type, [&] { return state.ApplySubject(*spec, value); });
        }
        if (const SHR::Settings::ProfileSpec *spec = SHR::Settings::FindProfile(name))
        {
            return commit(spec->Type, [&] { return state.ApplyProfile(*spec, value); });
        }

        // Loud: the layout and the registry disagree, and the edit went nowhere.
        SKSE::log::error(FMT_STRING("Menu setting {:s} is not in the registry"), name);
        return false;
    }

    std::optional<float> Read(const RE::BSFixedString &settingId, Kind kind)
    {
        const std::string_view name = AsView(settingId);

        if (const SHR::Settings::SubjectSpec *spec = SHR::Settings::FindSubject(name))
        {
            return spec->Type == kind
                ? std::optional<float>{ SHR::PluginState::Get().ReadSubject(spec->Field) }
                : std::nullopt;
        }
        if (const SHR::Settings::ProfileSpec *spec = SHR::Settings::FindProfile(name))
        {
            return spec->Type == kind
                ? std::optional<float>{ SHR::PluginState::ReadProfile(spec->Field) }
                : std::nullopt;
        }
        return std::nullopt;
    }

    // Both tables in one list: Papyrus type does not follow the scope split.
    std::vector<RE::BSFixedString> IdsOf(Kind kind)
    {
        std::vector<RE::BSFixedString> ids;
        for (const SHR::Settings::SubjectSpec &spec : SHR::Settings::SubjectSettings())
        {
            if (spec.Type == kind)
            {
                ids.emplace_back(spec.Id);
            }
        }
        for (const SHR::Settings::ProfileSpec &spec : SHR::Settings::ProfileSettings())
        {
            if (spec.Type == kind)
            {
                ids.emplace_back(spec.Id);
            }
        }
        return ids;
    }

    void SetFloat([[maybe_unused]] RE::StaticFunctionTag *tag, RE::BSFixedString settingId, float value)
    {
        Guard("SetFloat", false, [&] { return Write(settingId, Kind::Float, value); });
    }

    void SetBool([[maybe_unused]] RE::StaticFunctionTag *tag, RE::BSFixedString settingId, bool value)
    {
        Guard("SetBool", false, [&] { return Write(settingId, Kind::Bool, value ? 1.0F : 0.0F); });
    }

    void SetInt([[maybe_unused]] RE::StaticFunctionTag *tag, RE::BSFixedString settingId, std::int32_t value)
    {
        Guard("SetInt", false, [&] {
            return Write(settingId, Kind::Key, static_cast<float>(value));
        });
    }

    float GetFloat([[maybe_unused]] RE::StaticFunctionTag *tag, RE::BSFixedString settingId)
    {
        return Guard("GetFloat", 0.0F, [&] { return Read(settingId, Kind::Float).value_or(0.0F); });
    }

    bool GetBool([[maybe_unused]] RE::StaticFunctionTag *tag, RE::BSFixedString settingId)
    {
        return Guard("GetBool", false, [&] { return Read(settingId, Kind::Bool).value_or(0.0F) != 0.0F; });
    }

    std::int32_t GetInt([[maybe_unused]] RE::StaticFunctionTag *tag, RE::BSFixedString settingId)
    {
        return Guard("GetInt", 0, [&] {
            return static_cast<std::int32_t>(Read(settingId, Kind::Key).value_or(0.0F));
        });
    }

    void ResetSubject([[maybe_unused]] RE::StaticFunctionTag *tag)
    {
        Guard("ResetSubject", false, [] {
            SHR::PluginState::Get().ResetSubjectSettings();
            return true;
        });
    }

    void ResetProfile([[maybe_unused]] RE::StaticFunctionTag *tag)
    {
        Guard("ResetProfile", false, [] {
            SHR::PluginState::Get().ResetProfileSettings();
            return true;
        });
    }

    using IdList = std::vector<RE::BSFixedString>;

    IdList GetFloatIds([[maybe_unused]] RE::StaticFunctionTag *tag)
    {
        return Guard<IdList>("GetFloatIds", { }, [] { return IdsOf(Kind::Float); });
    }

    IdList GetBoolIds([[maybe_unused]] RE::StaticFunctionTag *tag)
    {
        return Guard<IdList>("GetBoolIds", { }, [] { return IdsOf(Kind::Bool); });
    }

    IdList GetIntIds([[maybe_unused]] RE::StaticFunctionTag *tag)
    {
        return Guard<IdList>("GetIntIds", { }, [] { return IdsOf(Kind::Key); });
    }

    bool RegisterFunctions(RE::BSScript::IVirtualMachine *vm)
    {
        vm->RegisterFunction("SetFloat", ScriptName, SetFloat);
        vm->RegisterFunction("SetBool", ScriptName, SetBool);
        vm->RegisterFunction("SetInt", ScriptName, SetInt);
        vm->RegisterFunction("GetFloat", ScriptName, GetFloat);
        vm->RegisterFunction("GetBool", ScriptName, GetBool);
        vm->RegisterFunction("GetInt", ScriptName, GetInt);
        vm->RegisterFunction("ResetSubject", ScriptName, ResetSubject);
        vm->RegisterFunction("ResetProfile", ScriptName, ResetProfile);
        vm->RegisterFunction("GetFloatIds", ScriptName, GetFloatIds);
        vm->RegisterFunction("GetBoolIds", ScriptName, GetBoolIds);
        vm->RegisterFunction("GetIntIds", ScriptName, GetIntIds);
        return true;
    }
}

void SHR::MenuBridge::Register()
{
    const auto *papyrus = SKSE::GetPapyrusInterface();

    // A failure here is worth a log line but not a fail-fast: it costs the menu, not the simulation.
    if (!papyrus || !papyrus->Register(RegisterFunctions))
    {
        SKSE::log::error(FMT_STRING("Failed to register {:s}"), ScriptName);
        return;
    }

    SKSE::log::info(FMT_STRING("Queued {:s} registration"), ScriptName);
}

void SHR::MenuBridge::Report(const char *when)
{
    auto *dataHandler = RE::TESDataHandler::GetSingleton();
    if (!dataHandler)
    {
        SKSE::log::warn(FMT_STRING("Menu {:s}: no data handler"), when);
        return;
    }

    const RE::TESFile *file = dataHandler->LookupLoadedLightModByName(PluginFile);
    if (!file)
    {
        file = dataHandler->LookupLoadedModByName(PluginFile);
    }
    if (!file)
    {
        SKSE::log::warn(FMT_STRING("Menu {:s}: {:s} is not loaded"), when, PluginFile);
        return;
    }

    auto *quest = dataHandler->LookupForm<RE::TESQuest>(QuestLocalID, PluginFile);
    if (!quest)
    {
        SKSE::log::warn(
            FMT_STRING("Menu {:s}: quest {:#x} absent from {:s}"),
            when,
            QuestLocalID,
            PluginFile
        );
        return;
    }

    SKSE::log::info(
        FMT_STRING("Menu {:s}: quest={:#010x} running={} stage={:d} aliasFilled={}"),
        when,
        quest->GetFormID(),
        quest->IsRunning(),
        quest->GetCurrentStageID(),
        quest->GetAliasedRef(0).get() != nullptr
    );
}
