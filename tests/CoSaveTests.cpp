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
#include "adapter/Settings.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

namespace
{
    using SHR::CoSave::Consequence;
    using SHR::CoSave::CoSaveRecords;
    using SHR::CoSave::RecordFamily;
    using SHR::CoSave::RecordVerdict;
    using SHR::CoSave::StateField;

    constexpr float Tolerance = 1e-4F;

    // Stands in for Runtime::ComputeEquilibriumContractility. No other field uses this value, so an
    // accidental fallback to it is visible.
    constexpr float Equilibrium = 0.77F;

    float ComputeEquilibrium(const SHR::SimulationState &) { return Equilibrium; }

    // Deliberately far from the persisted state below, so a field that fell back is never mistaken
    // for one that restored.
    SHR::SimulationState MakeInitial()
    {
        return {
            .FastHeartRate    = 33.0F,
            .SlowHeartRate    = 22.0F,
            .Exertion         = 1.2F,
            .Adrenaline       = 0.0F,
            .Contractility    = 0.0F,
            .Fitness          = 10.0F,
            .AcuteFatigue     = 0.0F,
            .LongTermFatigue  = 0.0F,
            .RespirationRate  = 14.0F,
            .RespirationDepth = 0.0F,
            .RespirationPhase = 0.0F,
            .DeathSeconds     = std::nullopt,
        };
    }

    // A mid-exertion character, every persisted field distinct and in domain.
    SHR::SimulationState MakePersisted()
    {
        return {
            .FastHeartRate    = 72.0F,
            .SlowHeartRate    = 48.0F,
            .Exertion         = 6.5F,
            .Adrenaline       = 0.35F,
            .Contractility    = 0.42F,
            .Fitness          = 14.0F,
            .AcuteFatigue     = 2.5F,
            .LongTermFatigue  = 1.25F,
            .RespirationRate  = 26.0F,
            .RespirationDepth = 0.6F,
            .RespirationPhase = 0.3F,   // not in the schema
            .DeathSeconds     = 12.0F,  // not in the schema
        };
    }

    std::uint32_t TypeOf(std::string_view name)
    {
        for (const SHR::CoSave::RecordSpec &spec : SHR::CoSave::KnownRecords())
        {
            if (name == spec.Name)
            {
                return spec.Type;
            }
        }
        FAIL("no record spec named " << name);
        return 0;
    }

    // The defaults are a well-formed record; each malformed case overrides only what makes it so.
    RecordVerdict Feed(
        CoSaveRecords  &records,
        std::string_view name,
        float            value,
        std::uint32_t    version   = 0,
        std::uint32_t    size      = sizeof(float),
        std::uint32_t    bytesRead = sizeof(float)
    )
    {
        return records.Accept(TypeOf(name), version, size, bytesRead, value);
    }

    // The v0.5.0 through v0.5.3 record set, taken from the writer so the two cannot disagree.
    void FeedCurrentRecordSet(CoSaveRecords &records, const SHR::SimulationState &state)
    {
        for (const SHR::CoSave::RecordValue &record : SHR::CoSave::RecordsToWrite(state))
        {
            CHECK(
                records.Accept(record.Type, record.Version, sizeof(float), sizeof(float), record.Value)
                == RecordVerdict::Accepted
            );
        }
    }

    SHR::SimulationState Restore(
        const CoSaveRecords &records,
        std::optional<float> liveDeathSeconds = std::nullopt
    )
    {
        return SHR::CoSave::RestoreSimulationState(
            records,
            MakeInitial(),
            liveDeathSeconds,
            ComputeEquilibrium
        );
    }
}

TEST_CASE("Every state field has exactly one record row", "[cosave]")
{
    std::array<int, SHR::CoSave::StateFieldCount> rows{ };
    for (const SHR::CoSave::RecordSpec &spec : SHR::CoSave::KnownRecords())
    {
        if (spec.Family == RecordFamily::State)
        {
            ++rows[static_cast<std::size_t>(spec.Field)];
        }
        // Round-tripping the type through the lookup is what the loader's dispatch depends on.
        CHECK(SHR::CoSave::FindRecord(spec.Type) == &spec);
    }

    for (const int count : rows)
    {
        CHECK(count == 1);
    }
}

TEST_CASE("The oldest released co-save carries only the heart-rate total", "[cosave]")
{
    // v0.4.0.0-alpha emitted PCHR and nothing else, so every other field must take its initial
    // value and the fast/slow split must be re-derived from the total.
    CoSaveRecords records;
    REQUIRE(Feed(records, "PCHR", 120.0F) == RecordVerdict::Accepted);

    const SHR::SimulationState initial  = MakeInitial();
    const SHR::SimulationState restored = Restore(records);

    const float fastFraction = initial.FastHeartRate / (initial.FastHeartRate + initial.SlowHeartRate);
    CHECK_THAT(restored.FastHeartRate, Catch::Matchers::WithinAbs(fastFraction * 120.0F, Tolerance));
    CHECK_THAT(
        restored.FastHeartRate + restored.SlowHeartRate,
        Catch::Matchers::WithinAbs(120.0F, Tolerance)
    );

    CHECK_THAT(restored.Exertion, Catch::Matchers::WithinAbs(initial.Exertion, Tolerance));
    CHECK_THAT(restored.Fitness, Catch::Matchers::WithinAbs(initial.Fitness, Tolerance));
    CHECK_THAT(restored.RespirationRate, Catch::Matchers::WithinAbs(initial.RespirationRate, Tolerance));
    // Absent contractility resumes at equilibrium, not at its initial value.
    CHECK_THAT(restored.Contractility, Catch::Matchers::WithinAbs(Equilibrium, Tolerance));
}

TEST_CASE("The current record set round-trips", "[cosave]")
{
    const SHR::SimulationState persisted = MakePersisted();

    CoSaveRecords records;
    FeedCurrentRecordSet(records, persisted);

    const SHR::SimulationState restored = Restore(records, persisted.DeathSeconds);

    CHECK_THAT(restored.FastHeartRate, Catch::Matchers::WithinAbs(persisted.FastHeartRate, Tolerance));
    CHECK_THAT(restored.SlowHeartRate, Catch::Matchers::WithinAbs(persisted.SlowHeartRate, Tolerance));
    CHECK_THAT(restored.Exertion, Catch::Matchers::WithinAbs(persisted.Exertion, Tolerance));
    CHECK_THAT(restored.Adrenaline, Catch::Matchers::WithinAbs(persisted.Adrenaline, Tolerance));
    CHECK_THAT(restored.Contractility, Catch::Matchers::WithinAbs(persisted.Contractility, Tolerance));
    CHECK_THAT(restored.Fitness, Catch::Matchers::WithinAbs(persisted.Fitness, Tolerance));
    CHECK_THAT(restored.AcuteFatigue, Catch::Matchers::WithinAbs(persisted.AcuteFatigue, Tolerance));
    CHECK_THAT(restored.LongTermFatigue, Catch::Matchers::WithinAbs(persisted.LongTermFatigue, Tolerance));
    CHECK_THAT(restored.RespirationRate, Catch::Matchers::WithinAbs(persisted.RespirationRate, Tolerance));
    CHECK_THAT(restored.RespirationDepth, Catch::Matchers::WithinAbs(persisted.RespirationDepth, Tolerance));

    // Outside the schema: the oscillator restarts and the death timer belongs to the live session.
    CHECK_THAT(restored.RespirationPhase, Catch::Matchers::WithinAbs(0.0F, Tolerance));
    REQUIRE(restored.DeathSeconds.has_value());
    CHECK_THAT(*restored.DeathSeconds, Catch::Matchers::WithinAbs(12.0F, Tolerance));
}

TEST_CASE("A malformed header is rejected before its payload is read", "[cosave]")
{
    CoSaveRecords records;

    SECTION("a record longer or shorter than its type declares")
    {
        CHECK(Feed(records, "FTNS", 14.0F, 0, sizeof(double)) == RecordVerdict::BadSize);
    }
    SECTION("a record written by a newer build")
    {
        CHECK(Feed(records, "FTNS", 14.0F, 1) == RecordVerdict::UnknownVersion);
    }
    SECTION("a record type this build has never known")
    {
        CHECK(records.Accept(0x4C545358, 0, sizeof(float), sizeof(float), 1.0F)
            == RecordVerdict::UnknownType);
    }

    // Whichever way it failed, the field never took a value.
    CHECK_FALSE(records.Get(StateField::Fitness).has_value());
}

TEST_CASE("A truncated payload does not become a value", "[cosave]")
{
    // A short read must not engage the destination, or it would be indistinguishable from a good one.
    CoSaveRecords records;
    CHECK(Feed(records, "FTNS", 14.0F, 0, sizeof(float), 2) == RecordVerdict::ShortRead);
    CHECK_FALSE(records.Get(StateField::Fitness).has_value());
}

TEST_CASE("A duplicate record keeps the first copy", "[cosave]")
{
    CoSaveRecords records;
    REQUIRE(Feed(records, "FTNS", 14.0F) == RecordVerdict::Accepted);
    CHECK(Feed(records, "FTNS", 99.0F) == RecordVerdict::Duplicate);

    REQUIRE(records.Get(StateField::Fitness).has_value());
    CHECK_THAT(*records.Get(StateField::Fitness), Catch::Matchers::WithinAbs(14.0F, Tolerance));
}

TEST_CASE("Values that cannot belong to a field are rejected", "[cosave]")
{
    CoSaveRecords records;

    // Every comparison against a NaN is false, so a sign test alone would let one through.
    CHECK(Feed(records, "FTNS", std::numeric_limits<float>::quiet_NaN()) == RecordVerdict::OutOfDomain);
    CHECK(Feed(records, "PCHR", std::numeric_limits<float>::infinity()) == RecordVerdict::OutOfDomain);
    CHECK(Feed(records, "RRTE", 0.0F) == RecordVerdict::OutOfDomain);
    CHECK(Feed(records, "AFTG", -1.0F) == RecordVerdict::OutOfDomain);

    CHECK_FALSE(records.Get(StateField::Fitness).has_value());
    CHECK_FALSE(records.Get(StateField::HeartRate).has_value());
    CHECK_FALSE(records.Get(StateField::RespirationRate).has_value());
    CHECK_FALSE(records.Get(StateField::AcuteFatigue).has_value());
}

TEST_CASE("A finite value outside a closed range is clamped, not dropped", "[cosave]")
{
    // Wrong magnitude rather than wrong kind: clamping keeps the rest of the restore.
    CoSaveRecords records;
    CHECK(Feed(records, "CTLY", 1.5F) == RecordVerdict::Accepted);
    CHECK(Feed(records, "RDPT", -0.25F) == RecordVerdict::Accepted);

    REQUIRE(records.Get(StateField::Contractility).has_value());
    CHECK_THAT(*records.Get(StateField::Contractility), Catch::Matchers::WithinAbs(1.0F, Tolerance));
    REQUIRE(records.Get(StateField::RespirationDepth).has_value());
    CHECK_THAT(*records.Get(StateField::RespirationDepth), Catch::Matchers::WithinAbs(0.0F, Tolerance));
}

TEST_CASE("One bad record costs one field", "[cosave]")
{
    // Field-by-field fallback: a single short float must not cost accumulated fitness too.
    const SHR::SimulationState persisted = MakePersisted();
    const SHR::SimulationState initial   = MakeInitial();

    CoSaveRecords records;
    for (const SHR::CoSave::RecordValue &record : SHR::CoSave::RecordsToWrite(persisted))
    {
        // Truncate exactly one record; every other one arrives intact.
        const bool truncated = std::string_view(record.Name) == "AFTG";
        records.Accept(
            record.Type,
            record.Version,
            sizeof(float),
            truncated ? 2 : sizeof(float),
            record.Value
        );
    }

    const SHR::SimulationState restored = Restore(records);

    CHECK_THAT(restored.AcuteFatigue, Catch::Matchers::WithinAbs(initial.AcuteFatigue, Tolerance));
    CHECK_THAT(restored.Fitness, Catch::Matchers::WithinAbs(persisted.Fitness, Tolerance));
    CHECK_THAT(restored.LongTermFatigue, Catch::Matchers::WithinAbs(persisted.LongTermFatigue, Tolerance));
    CHECK_THAT(restored.FastHeartRate, Catch::Matchers::WithinAbs(persisted.FastHeartRate, Tolerance));
}

TEST_CASE("What a dropped record costs depends on its family", "[cosave]")
{
    // Absence never reaches here, so every case below is a record that arrived and was dropped.
    CHECK(SHR::CoSave::ConsequenceOf(RecordFamily::State, RecordVerdict::Accepted)
        == Consequence::None);
    CHECK(SHR::CoSave::ConsequenceOf(RecordFamily::Override, RecordVerdict::Accepted)
        == Consequence::None);

    // An unknown type belongs to no family, so forward compatibility wins over either fallback.
    CHECK(SHR::CoSave::ConsequenceOf(RecordFamily::State, RecordVerdict::UnknownType)
        == Consequence::RecordIgnored);
    CHECK(SHR::CoSave::ConsequenceOf(RecordFamily::Override, RecordVerdict::UnknownType)
        == Consequence::RecordIgnored);

    constexpr RecordVerdict Malformed[] = {
        RecordVerdict::UnknownVersion,
        RecordVerdict::BadSize,
        RecordVerdict::ShortRead,
        RecordVerdict::Duplicate,
        RecordVerdict::OutOfDomain,
    };
    for (const RecordVerdict verdict : Malformed)
    {
        CHECK(SHR::CoSave::ConsequenceOf(RecordFamily::State, verdict) == Consequence::ProgressionLost);
        CHECK(SHR::CoSave::ConsequenceOf(RecordFamily::Override, verdict) == Consequence::OverrideDropped);
    }
}

TEST_CASE("Every verdict and consequence has diagnostic wording", "[cosave]")
{
    constexpr RecordVerdict Verdicts[] = {
        RecordVerdict::Accepted,
        RecordVerdict::UnknownType,
        RecordVerdict::UnknownVersion,
        RecordVerdict::BadSize,
        RecordVerdict::ShortRead,
        RecordVerdict::Duplicate,
        RecordVerdict::OutOfDomain,
    };
    for (const RecordVerdict verdict : Verdicts)
    {
        CHECK(std::string_view(SHR::CoSave::Describe(verdict)) != "unrecognized verdict");
    }

    constexpr Consequence Consequences[] = {
        Consequence::None,
        Consequence::ProgressionLost,
        Consequence::OverrideDropped,
        Consequence::RecordIgnored,
    };
    for (const Consequence consequence : Consequences)
    {
        CHECK(std::string_view(SHR::CoSave::Describe(consequence)) != "unrecognized consequence");
    }
}

TEST_CASE("A persisted contractility is used instead of the equilibrium fallback", "[cosave]")
{
    CoSaveRecords records;
    REQUIRE(Feed(records, "CTLY", 0.42F) == RecordVerdict::Accepted);

    CHECK_THAT(Restore(records).Contractility, Catch::Matchers::WithinAbs(0.42F, Tolerance));
}

TEST_CASE("Override records round-trip through the co-save", "[cosave][settings]")
{
    SHR::Settings::Overrides written;
    REQUIRE(written.Set(SHR::Settings::Subject::RestingHeartRate, 47.0F));
    REQUIRE(written.Set(SHR::Settings::Subject::FitnessMaxMets, 62.0F));

    const auto records = SHR::CoSave::OverrideRecordsToWrite(written);
    REQUIRE(records.size() == 2);

    CoSaveRecords read;
    for (const auto &record : records)
    {
        REQUIRE(
            read.Accept(record.Type, record.Version, sizeof(float), sizeof(float), record.Value) ==
            RecordVerdict::Accepted
        );
    }

    CHECK_THAT(
        *read.Overrides().Get(SHR::Settings::Subject::RestingHeartRate),
        Catch::Matchers::WithinAbs(47.0F, Tolerance)
    );
    CHECK_FALSE(read.Overrides().Get(SHR::Settings::Subject::ArrhythmiaSusceptibility).has_value());
}

TEST_CASE("A setting nobody moved emits no record", "[cosave][settings]")
{
    const SHR::Settings::Overrides untouched;
    CHECK(SHR::CoSave::OverrideRecordsToWrite(untouched).empty());
}

TEST_CASE("A malformed override is dropped rather than applied", "[cosave][settings]")
{
    const auto *resting = SHR::Settings::FindSubject(SHR::Settings::Subject::RestingHeartRate);
    REQUIRE(resting != nullptr);

    SECTION("out of domain")
    {
        CoSaveRecords records;
        const RecordVerdict verdict =
            records.Accept(resting->Record, 0, sizeof(float), sizeof(float), resting->Max + 10.0F);

        CHECK(verdict == RecordVerdict::OutOfDomain);
        CHECK_FALSE(records.Overrides().Get(SHR::Settings::Subject::RestingHeartRate).has_value());
        CHECK(
            SHR::CoSave::ConsequenceOf(RecordFamily::Override, verdict) ==
            Consequence::OverrideDropped
        );
    }

    SECTION("not a number")
    {
        CoSaveRecords records;
        CHECK(
            records.Accept(
                resting->Record, 0, sizeof(float), sizeof(float),
                std::numeric_limits<float>::quiet_NaN()
            ) == RecordVerdict::OutOfDomain
        );
    }

    SECTION("written by a newer build")
    {
        CoSaveRecords records;
        CHECK(
            records.Accept(resting->Record, 1, sizeof(float), sizeof(float), 50.0F) ==
            RecordVerdict::UnknownVersion
        );
    }

    SECTION("wrong size")
    {
        CoSaveRecords records;
        CHECK(
            records.Accept(resting->Record, 0, sizeof(double), sizeof(double), 50.0F) ==
            RecordVerdict::BadSize
        );
    }

    SECTION("arrives twice")
    {
        CoSaveRecords records;
        REQUIRE(
            records.Accept(resting->Record, 0, sizeof(float), sizeof(float), 50.0F) ==
            RecordVerdict::Accepted
        );
        CHECK(
            records.Accept(resting->Record, 0, sizeof(float), sizeof(float), 51.0F) ==
            RecordVerdict::Duplicate
        );
    }
}

TEST_CASE("A record type resolves to its family and name", "[cosave][settings]")
{
    const auto *resting = SHR::Settings::FindSubject(SHR::Settings::Subject::RestingHeartRate);
    REQUIRE(resting != nullptr);

    const auto setting = SHR::CoSave::Identify(resting->Record);
    REQUIRE(setting.has_value());
    CHECK(setting->Family == RecordFamily::Override);
    CHECK(std::string_view{ setting->Name } == resting->Name);

    const SHR::CoSave::RecordSpec &first = SHR::CoSave::KnownRecords().front();
    const auto state = SHR::CoSave::Identify(first.Type);
    REQUIRE(state.has_value());
    CHECK(state->Family == RecordFamily::State);
    CHECK(std::string_view{ state->Name } == first.Name);

    CHECK_FALSE(SHR::CoSave::Identify(0xDEADBEEF).has_value());
}

// The reader loop classifies by type alone, so an override's framing has to be reachable without a
// record-table row to look it up in.
TEST_CASE("Header classification covers both families", "[cosave][settings]")
{
    const auto *resting = SHR::Settings::FindSubject(SHR::Settings::Subject::RestingHeartRate);
    REQUIRE(resting != nullptr);

    using SHR::CoSave::ClassifyHeader;
    CHECK(ClassifyHeader(resting->Record, 0, sizeof(float)) == RecordVerdict::Accepted);
    CHECK(ClassifyHeader(resting->Record, 1, sizeof(float)) == RecordVerdict::UnknownVersion);
    CHECK(ClassifyHeader(resting->Record, 0, sizeof(double)) == RecordVerdict::BadSize);
    CHECK(ClassifyHeader(0xDEADBEEF, 0, sizeof(float)) == RecordVerdict::UnknownType);

    const SHR::CoSave::RecordSpec &state = SHR::CoSave::KnownRecords().front();
    CHECK(ClassifyHeader(state.Type, state.KnownVersion, state.ExpectedSize) == RecordVerdict::Accepted);
    CHECK(ClassifyHeader(state.Type, state.KnownVersion + 1, state.ExpectedSize) ==
        RecordVerdict::UnknownVersion);
}
