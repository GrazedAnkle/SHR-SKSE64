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
#pragma once

#include "adapter/Settings.hpp"
#include "core/SimulationState.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace SHR::CoSave
{
    // SKSE-free co-save record policy; ARCHITECTURE.md owns why it sits on this side of the boundary.

    enum class RecordFamily
    {
        State,     // absent means "use the initial value"
        Override,  // absent means "the player never moved this control"
    };

    enum class RecordVerdict
    {
        Accepted,
        UnknownType,     // not in the table; skipped for forward compatibility
        UnknownVersion,  // written by a newer SHR than this one
        BadSize,         // the stream's record length is not what the type declares
        ShortRead,       // the stream returned fewer bytes than the record claims to hold
        Duplicate,       // the type already arrived once this load
        OutOfDomain,     // read cleanly, but the value cannot be a value of this field
    };

    // What a dropped record costs. Absence never reaches here: it is not an event in either family.
    enum class Consequence
    {
        None,
        ProgressionLost,   // a State field reverts to its initial value
        OverrideDropped,   // a player's setting silently stops applying
        RecordIgnored,     // an unknown record, left alone so newer saves stay loadable
    };

    // Note the asymmetry: a sign constraint rejects, a closed range clamps.
    enum class ValueDomain
    {
        Positive,
        NonNegative,
        UnitInterval,
    };

    // The SimulationState field a State record carries. Count must stay last.
    enum class StateField : std::size_t
    {
        HeartRate,
        FastHeartRate,
        Exertion,
        Adrenaline,
        Fitness,
        AcuteFatigue,
        LongTermFatigue,
        RespirationRate,
        Contractility,
        RespirationDepth,
        Count,
    };

    inline constexpr std::size_t StateFieldCount = static_cast<std::size_t>(StateField::Count);

    // One row of the record table. Reader dispatch, writer emission, and validation all read this.
    struct RecordSpec
    {
        std::uint32_t Type;          // as written to the stream, byte-swapped from the 4CC
        const char   *Name;          // the 4CC as text, for diagnostics
        RecordFamily  Family;
        std::uint32_t KnownVersion;  // a higher version means a newer SHR wrote it
        std::uint32_t ExpectedSize;
        ValueDomain   Domain;
        StateField    Field;         // meaningful only for RecordFamily::State
    };

    // Every state record this build knows. Overrides are not here; Identify covers both families.
    std::span<const RecordSpec> KnownRecords() noexcept;

    const RecordSpec *FindRecord(std::uint32_t type) noexcept;

    // What the reader needs before it has read anything: which family's rules apply, and a name.
    struct RecordIdentity
    {
        RecordFamily Family;
        const char  *Name;  // the 4CC as text
    };

    // Override rows are not in the table: the settings registry owns their 4CCs, so the two cannot
    // drift. Returns nullopt for a type this build does not know at all.
    std::optional<RecordIdentity> Identify(std::uint32_t type) noexcept;

    // Callers must clear this before consuming a payload, so a malformed record costs only its header.
    RecordVerdict ClassifyHeader(const RecordSpec *spec, std::uint32_t version, std::uint32_t size) noexcept;

    // Both families, so one reader loop serves both.
    RecordVerdict ClassifyHeader(std::uint32_t type, std::uint32_t version, std::uint32_t size) noexcept;

    Consequence ConsequenceOf(RecordFamily family, RecordVerdict verdict) noexcept;

    // Wording only; the caller owns the log, so this layer stays free of side effects for the tests.
    const char *Describe(RecordVerdict verdict) noexcept;
    const char *Describe(Consequence consequence) noexcept;

    std::optional<float> ValidateDomain(ValueDomain domain, float value) noexcept;

    // A field is engaged only if its record was present, well-framed, fully read, and in domain.
    class CoSaveRecords
    {
    public:
        // `bytesRead` is the serialization interface's read result. Duplicates are first-wins: the
        // writer never emits one, so a second copy means corruption.
        RecordVerdict Accept(
            std::uint32_t type,
            std::uint32_t version,
            std::uint32_t size,
            std::uint32_t bytesRead,
            float         value
        ) noexcept;

        std::optional<float> Get(StateField field) const noexcept;

        const Settings::Overrides &Overrides() const noexcept { return m_Overrides; }

        static constexpr std::uint32_t OverrideVersion = 0;

    private:
        // Framing is already cleared by the time this runs.
        RecordVerdict AcceptOverride(std::uint32_t type, float value) noexcept;

        std::array<std::optional<float>, StateFieldCount> m_Fields{ };
        Settings::Overrides                              m_Overrides{ };
    };

    // `initial` supplies the per-field fallbacks, `liveDeathSeconds` the value the schema does not
    // persist. The equilibrium contractility is a callable because that fallback depends on the
    // restored state and lives on Runtime, which this layer cannot see.
    SimulationState RestoreSimulationState(
        const CoSaveRecords   &records,
        const SimulationState &initial,
        std::optional<float>   liveDeathSeconds,
        const std::function<float(const SimulationState &)> &computeEquilibriumContractility
    );

    struct RecordValue
    {
        std::uint32_t Type;
        std::uint32_t Version;
        const char   *Name;
        float         Value;
    };

    // What a save must emit for this state, derived from the same table the reader dispatches on.
    std::array<RecordValue, StateFieldCount> RecordsToWrite(const SimulationState &state);

    // Only the settings a player actually moved, so an untouched one stays absent rather than
    // being pinned to whatever the profile default happened to be at save time.
    std::vector<RecordValue> OverrideRecordsToWrite(const Settings::Overrides &overrides);
}
