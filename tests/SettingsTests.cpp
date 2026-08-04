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
#include "core/Constants.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <limits>
#include <set>
#include <string_view>
#include <vector>

namespace
{
    constexpr float Tolerance = 1.0e-5F;
}

TEST_CASE("Every setting has exactly one row", "[settings]")
{
    CHECK(SHR::Settings::SubjectSettings().size() == SHR::Settings::SubjectCount);
    CHECK(SHR::Settings::ProfileSettings().size() == SHR::Settings::ProfileCount);

    std::set<std::size_t> subjectFields;
    for (const auto &spec : SHR::Settings::SubjectSettings())
    {
        CHECK(subjectFields.insert(static_cast<std::size_t>(spec.Field)).second);
    }

    std::set<std::size_t> profileFields;
    for (const auto &spec : SHR::Settings::ProfileSettings())
    {
        CHECK(profileFields.insert(static_cast<std::size_t>(spec.Field)).second);
    }
}

TEST_CASE("Setting ids and record types are unique", "[settings]")
{
    std::set<std::string_view> ids;
    std::set<std::uint32_t>    records;

    for (const auto &spec : SHR::Settings::SubjectSettings())
    {
        CHECK(ids.insert(spec.Id).second);
        CHECK(records.insert(spec.Record).second);
    }
    for (const auto &spec : SHR::Settings::ProfileSettings())
    {
        CHECK(ids.insert(spec.Id).second);
    }
}

TEST_CASE("Override record types do not collide with state records", "[settings][cosave]")
{
    for (const auto &setting : SHR::Settings::SubjectSettings())
    {
        INFO(setting.Name);
        CHECK(SHR::CoSave::FindRecord(setting.Record) == nullptr);
    }
}

TEST_CASE("Compiled defaults sit inside their own declared domains", "[settings]")
{
    const SHR::RuntimeSettings settings;
    for (const auto &spec : SHR::Settings::SubjectSettings())
    {
        INFO(spec.Id);
        CHECK(SHR::Settings::InDomain(spec, SHR::Settings::Read(settings, spec.Field)));
    }

    const SHR::Config config;
    for (const auto &spec : SHR::Settings::ProfileSettings())
    {
        INFO(spec.Id);
        CHECK(SHR::Settings::InDomain(spec, SHR::Settings::Read(config, spec.Field)));
    }
}

TEST_CASE("A setting round-trips through apply and read", "[settings]")
{
    SHR::RuntimeSettings settings;
    const auto *resting = SHR::Settings::FindSubject("fRestingHeartRate:Subject");
    REQUIRE(resting != nullptr);

    REQUIRE(SHR::Settings::Apply(settings, *resting, 62.0F));
    CHECK_THAT(
        SHR::Settings::Read(settings, SHR::Settings::Subject::RestingHeartRate),
        Catch::Matchers::WithinAbs(62.0F, Tolerance)
    );

    SHR::Config config;
    const auto *key = SHR::Settings::FindProfile("iListenKey:Profile");
    REQUIRE(key != nullptr);

    REQUIRE(SHR::Settings::Apply(config, *key, 47.0F));
    CHECK(config.Input.Listen == 47U);
}

TEST_CASE("An out-of-domain value leaves the settings untouched", "[settings]")
{
    SHR::RuntimeSettings settings;
    const float original = settings.Simulation.RestingHeartRate;

    const auto *resting = SHR::Settings::FindSubject("fRestingHeartRate:Subject");
    REQUIRE(resting != nullptr);

    CHECK_FALSE(SHR::Settings::Apply(settings, *resting, resting->Max + 1.0F));
    CHECK_FALSE(SHR::Settings::Apply(settings, *resting, resting->Min - 1.0F));
    CHECK_FALSE(SHR::Settings::Apply(settings, *resting, std::numeric_limits<float>::quiet_NaN()));
    CHECK_FALSE(SHR::Settings::Apply(settings, *resting, std::numeric_limits<float>::infinity()));

    CHECK_THAT(
        settings.Simulation.RestingHeartRate,
        Catch::Matchers::WithinAbs(original, Tolerance)
    );
}

TEST_CASE("An unknown id resolves to no setting", "[settings]")
{
    CHECK(SHR::Settings::FindSubject("fNoSuchSetting:Subject") == nullptr);
    CHECK(SHR::Settings::FindProfile("fNoSuchSetting:Profile") == nullptr);
    // A subject id must not resolve through the profile table or the dispatch would cross scopes.
    CHECK(SHR::Settings::FindProfile("fRestingHeartRate:Subject") == nullptr);
}

TEST_CASE("An unset override resolves to the profile default", "[settings]")
{
    const SHR::RuntimeSettings defaults;
    const SHR::Settings::Overrides none;

    const SHR::RuntimeSettings resolved = none.Resolve(defaults);

    CHECK_FALSE(none.Any());
    CHECK_THAT(
        resolved.Simulation.RestingHeartRate,
        Catch::Matchers::WithinAbs(defaults.Simulation.RestingHeartRate, Tolerance)
    );
    CHECK_THAT(
        resolved.Simulation.FitnessMaxMets,
        Catch::Matchers::WithinAbs(defaults.Simulation.FitnessMaxMets, Tolerance)
    );
}

TEST_CASE("An override layers over the default and clears back to it", "[settings]")
{
    const SHR::RuntimeSettings defaults;
    SHR::Settings::Overrides overrides;

    REQUIRE(overrides.Set(SHR::Settings::Subject::RestingHeartRate, 47.0F));
    CHECK(overrides.Any());

    SHR::RuntimeSettings resolved = overrides.Resolve(defaults);
    CHECK_THAT(resolved.Simulation.RestingHeartRate, Catch::Matchers::WithinAbs(47.0F, Tolerance));
    // A setting nobody moved must not drift because a neighbour did.
    CHECK_THAT(
        resolved.Simulation.MaximumHeartRate,
        Catch::Matchers::WithinAbs(defaults.Simulation.MaximumHeartRate, Tolerance)
    );

    overrides.Clear(SHR::Settings::Subject::RestingHeartRate);
    resolved = overrides.Resolve(defaults);

    CHECK_FALSE(overrides.Any());
    CHECK_THAT(
        resolved.Simulation.RestingHeartRate,
        Catch::Matchers::WithinAbs(defaults.Simulation.RestingHeartRate, Tolerance)
    );
}

TEST_CASE("The ceiling override converts out of the unit it is stored in", "[settings]")
{
    SHR::Settings::Overrides overrides;
    REQUIRE(overrides.Set(SHR::Settings::Subject::FitnessMaxMets, 70.0F));

    const SHR::RuntimeSettings resolved = overrides.Resolve(SHR::RuntimeSettings{ });

    CHECK_THAT(
        resolved.Simulation.FitnessMaxMets,
        Catch::Matchers::WithinAbs(70.0F / SHR::Constants::MetsToVO2, Tolerance)
    );
}

TEST_CASE("An out-of-domain override is refused rather than stored", "[settings]")
{
    SHR::Settings::Overrides overrides;
    CHECK_FALSE(overrides.Set(SHR::Settings::Subject::FitnessMaxMets, 200.0F));
    CHECK_FALSE(overrides.Get(SHR::Settings::Subject::FitnessMaxMets).has_value());
}

// Dispatch tries the per-character table and then the profile one, so a shared id would silently
// resolve to whichever came first.
TEST_CASE("Setting ids are unique across both tables", "[settings]")
{
    std::vector<std::string_view> ids;
    for (const SHR::Settings::SubjectSpec &spec : SHR::Settings::SubjectSettings())
    {
        ids.push_back(spec.Id);
    }
    for (const SHR::Settings::ProfileSpec &spec : SHR::Settings::ProfileSettings())
    {
        ids.push_back(spec.Id);
    }

    std::ranges::sort(ids);
    CHECK(std::ranges::adjacent_find(ids) == ids.end());
}
