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
#include "adapter/CoSave.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iterator>

namespace
{
    using SHR::CoSave::RecordFamily;
    using SHR::CoSave::RecordSpec;
    using SHR::CoSave::StateField;
    using SHR::CoSave::ValueDomain;

    // A 4CC literal is an int, so the swap is wrapped: the widening happens in the parameter, where
    // it is a conversion rather than a narrowing initialization.
    consteval std::uint32_t RecordType(std::uint32_t fourCC) noexcept
    {
        return std::byteswap(fourCC);
    }

    // Wire format, matching what the released writer produced. Not adjustable.
    constexpr RecordSpec KnownRecordTable[] = {
        { RecordType('PCHR'), "PCHR", RecordFamily::State, 0, sizeof(float), ValueDomain::Positive,     StateField::HeartRate },
        { RecordType('FAHR'), "FAHR", RecordFamily::State, 0, sizeof(float), ValueDomain::Positive,     StateField::FastHeartRate },
        { RecordType('EXRT'), "EXRT", RecordFamily::State, 0, sizeof(float), ValueDomain::NonNegative,  StateField::Exertion },
        { RecordType('ADRL'), "ADRL", RecordFamily::State, 0, sizeof(float), ValueDomain::NonNegative,  StateField::Adrenaline },
        { RecordType('FTNS'), "FTNS", RecordFamily::State, 0, sizeof(float), ValueDomain::Positive,     StateField::Fitness },
        { RecordType('AFTG'), "AFTG", RecordFamily::State, 0, sizeof(float), ValueDomain::NonNegative,  StateField::AcuteFatigue },
        { RecordType('LFTG'), "LFTG", RecordFamily::State, 0, sizeof(float), ValueDomain::NonNegative,  StateField::LongTermFatigue },
        { RecordType('RRTE'), "RRTE", RecordFamily::State, 0, sizeof(float), ValueDomain::Positive,     StateField::RespirationRate },
        { RecordType('CTLY'), "CTLY", RecordFamily::State, 0, sizeof(float), ValueDomain::UnitInterval, StateField::Contractility },
        { RecordType('RDPT'), "RDPT", RecordFamily::State, 0, sizeof(float), ValueDomain::UnitInterval, StateField::RespirationDepth },
    };

    // Counted rather than compared against the table size, so an Override row does not trip an
    // assertion about State rows. What must hold is that RecordsToWrite fills its array exactly.
    consteval std::size_t StateRowCount() noexcept
    {
        std::size_t count = 0;
        for (const RecordSpec &spec : KnownRecordTable)
        {
            count += spec.Family == RecordFamily::State ? 1 : 0;
        }
        return count;
    }

    static_assert(
        StateRowCount() == SHR::CoSave::StateFieldCount,
        "every StateField needs exactly one State record row"
    );

    float ValueFor(StateField field, const SHR::SimulationState &state)
    {
        switch (field)
        {
        // Predates the fast/slow split, so it persists their sum rather than a component.
        case StateField::HeartRate:        return state.FastHeartRate + state.SlowHeartRate;
        case StateField::FastHeartRate:    return state.FastHeartRate;
        case StateField::Exertion:         return state.Exertion;
        case StateField::Adrenaline:       return state.Adrenaline;
        case StateField::Fitness:          return state.Fitness;
        case StateField::AcuteFatigue:     return state.AcuteFatigue;
        case StateField::LongTermFatigue:  return state.LongTermFatigue;
        case StateField::RespirationRate:  return state.RespirationRate;
        case StateField::Contractility:    return state.Contractility;
        case StateField::RespirationDepth: return state.RespirationDepth;
        case StateField::Count:            break;
        }
        return 0.0F;
    }
}

std::span<const SHR::CoSave::RecordSpec> SHR::CoSave::KnownRecords() noexcept
{
    return KnownRecordTable;
}

const SHR::CoSave::RecordSpec *SHR::CoSave::FindRecord(std::uint32_t type) noexcept
{
    const auto match = std::ranges::find(KnownRecordTable, type, &RecordSpec::Type);
    return match != std::ranges::end(KnownRecordTable) ? &*match : nullptr;
}

SHR::CoSave::RecordVerdict SHR::CoSave::ClassifyHeader(
    const RecordSpec *spec,
    std::uint32_t     version,
    std::uint32_t     size
) noexcept
{
    if (spec == nullptr)
    {
        return RecordVerdict::UnknownType;
    }
    // A newer SHR may have widened the record, so its payload cannot be assumed to mean what this
    // build would read.
    if (version > spec->KnownVersion)
    {
        return RecordVerdict::UnknownVersion;
    }
    if (size != spec->ExpectedSize)
    {
        return RecordVerdict::BadSize;
    }
    return RecordVerdict::Accepted;
}

SHR::CoSave::Consequence SHR::CoSave::ConsequenceOf(
    RecordFamily  family,
    RecordVerdict verdict
) noexcept
{
    if (verdict == RecordVerdict::Accepted)
    {
        return Consequence::None;
    }
    if (verdict == RecordVerdict::UnknownType)
    {
        return Consequence::RecordIgnored;
    }
    return family == RecordFamily::State
        ? Consequence::ProgressionLost
        : Consequence::OverrideDropped;
}

const char *SHR::CoSave::Describe(RecordVerdict verdict) noexcept
{
    switch (verdict)
    {
    case RecordVerdict::Accepted:       return "accepted";
    case RecordVerdict::UnknownType:    return "unknown record type";
    case RecordVerdict::UnknownVersion: return "record version is newer than this build knows";
    case RecordVerdict::BadSize:        return "record length does not match the type";
    case RecordVerdict::ShortRead:      return "record payload was truncated";
    case RecordVerdict::Duplicate:      return "record type already appeared in this co-save";
    case RecordVerdict::OutOfDomain:    return "value cannot belong to this field";
    }
    return "unrecognized verdict";
}

const char *SHR::CoSave::Describe(Consequence consequence) noexcept
{
    switch (consequence)
    {
    case Consequence::None:            return "no effect";
    case Consequence::ProgressionLost: return "the field falls back to its initial value";
    case Consequence::OverrideDropped: return "the setting reverts to the profile default";
    case Consequence::RecordIgnored:   return "the record is skipped";
    }
    return "unrecognized consequence";
}

std::optional<float> SHR::CoSave::ValidateDomain(ValueDomain domain, float value) noexcept
{
    // Ordered before the comparisons: every comparison against a NaN is false, so a sign test alone
    // would pass one through.
    if (!std::isfinite(value))
    {
        return std::nullopt;
    }

    switch (domain)
    {
    case ValueDomain::Positive:
        return value > 0.0F ? std::optional<float>{ value } : std::nullopt;
    case ValueDomain::NonNegative:
        return value >= 0.0F ? std::optional<float>{ value } : std::nullopt;
    case ValueDomain::UnitInterval:
        return std::clamp(value, 0.0F, 1.0F);
    }
    return std::nullopt;
}

SHR::CoSave::RecordVerdict SHR::CoSave::CoSaveRecords::Accept(
    std::uint32_t type,
    std::uint32_t version,
    std::uint32_t size,
    std::uint32_t bytesRead,
    float         value
) noexcept
{
    const RecordSpec   *spec   = FindRecord(type);
    const RecordVerdict header = ClassifyHeader(spec, version, size);
    if (header != RecordVerdict::Accepted)
    {
        return header;
    }

    if (bytesRead != spec->ExpectedSize)
    {
        return RecordVerdict::ShortRead;
    }

    if (spec->Family != RecordFamily::State)
    {
        // Classified, but there is nowhere to put the value until WI-034 adds override storage.
        return RecordVerdict::Accepted;
    }

    const auto index = static_cast<std::size_t>(spec->Field);
    if (m_Fields[index].has_value())
    {
        return RecordVerdict::Duplicate;
    }

    const std::optional<float> validated = ValidateDomain(spec->Domain, value);
    if (!validated)
    {
        return RecordVerdict::OutOfDomain;
    }

    m_Fields[index] = *validated;
    return RecordVerdict::Accepted;
}

std::optional<float> SHR::CoSave::CoSaveRecords::Get(StateField field) const noexcept
{
    const auto index = static_cast<std::size_t>(field);
    return index < StateFieldCount ? m_Fields[index] : std::nullopt;
}

SHR::SimulationState SHR::CoSave::RestoreSimulationState(
    const CoSaveRecords   &records,
    const SimulationState &initial,
    std::optional<float>   liveDeathSeconds,
    const std::function<float(const SimulationState &)> &computeEquilibriumContractility
)
{
    SimulationState state = initial;

    const float initialHeartRate = initial.FastHeartRate + initial.SlowHeartRate;
    const float heartRate = records.Get(StateField::HeartRate).value_or(initialHeartRate);

    // Re-derived for a save older than the fast/slow split. The fraction comes from the initial state
    // rather than HRFastFraction so an overridden coefficient cannot make the restored split disagree
    // with the model that will step it. The guard keeps a broken configuration from restoring a NaN.
    const float fastFraction = initialHeartRate > 0.0F
        ? initial.FastHeartRate / initialHeartRate
        : 0.0F;
    state.FastHeartRate = records.Get(StateField::FastHeartRate).value_or(fastFraction * heartRate);
    state.SlowHeartRate = heartRate - state.FastHeartRate;

    state.Exertion        = records.Get(StateField::Exertion).value_or(initial.Exertion);
    state.Adrenaline      = records.Get(StateField::Adrenaline).value_or(initial.Adrenaline);
    state.Fitness         = records.Get(StateField::Fitness).value_or(initial.Fitness);
    state.AcuteFatigue    = records.Get(StateField::AcuteFatigue).value_or(initial.AcuteFatigue);
    state.LongTermFatigue = records.Get(StateField::LongTermFatigue).value_or(initial.LongTermFatigue);
    state.RespirationRate = records.Get(StateField::RespirationRate).value_or(initial.RespirationRate);
    state.RespirationDepth =
        records.Get(StateField::RespirationDepth).value_or(initial.RespirationDepth);

    // Equilibrium rather than the initial value, which would contradict the heart rate beside it.
    const std::optional<float> contractility = records.Get(StateField::Contractility);
    state.Contractility = contractility
        ? *contractility
        : computeEquilibriumContractility(state);

    // Neither field is in the schema; the death timer belongs to the live session, not the save.
    state.RespirationPhase = initial.RespirationPhase;
    state.DeathSeconds     = liveDeathSeconds;
    return state;
}

std::array<SHR::CoSave::RecordValue, SHR::CoSave::StateFieldCount>
SHR::CoSave::RecordsToWrite(const SimulationState &state)
{
    std::array<RecordValue, StateFieldCount> values{ };
    std::size_t next = 0;
    for (const RecordSpec &spec : KnownRecordTable)
    {
        if (spec.Family != RecordFamily::State)
        {
            continue;
        }
        values[next++] = {
            .Type    = spec.Type,
            .Version = spec.KnownVersion,
            .Name    = spec.Name,
            .Value   = ValueFor(spec.Field, state),
        };
    }
    return values;
}
