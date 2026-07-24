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
#include "Constants.hpp"
#include "RhythmEngine.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace
{
    namespace C = SHR::Constants;
    constexpr float Tolerance = 1.0e-5F;

    struct UniformDraw
    {
        float Min;
        float Max;
        float Value;
    };

    class ScriptedRandom
    {
    public:
        ScriptedRandom(std::vector<UniformDraw> uniform, std::vector<float> normal = { })
            : m_Uniform(std::move(uniform))
            , m_Normal(std::move(normal))
        {
        }

        SHR::RhythmRandom Callbacks()
        {
            return {
                .Uniform = [this](float min, float max) {
                    REQUIRE(m_UniformIndex < m_Uniform.size());
                    const UniformDraw draw = m_Uniform[m_UniformIndex++];
                    CHECK_THAT(min, Catch::Matchers::WithinAbs(draw.Min, Tolerance));
                    CHECK_THAT(max, Catch::Matchers::WithinAbs(draw.Max, Tolerance));
                    CHECK(draw.Value >= min);
                    CHECK(draw.Value <= max);
                    return draw.Value;
                },
                .StandardNormal = [this]() {
                    REQUIRE(m_NormalIndex < m_Normal.size());
                    return m_Normal[m_NormalIndex++];
                },
            };
        }

        void RequireConsumed() const
        {
            REQUIRE(m_UniformIndex == m_Uniform.size());
            REQUIRE(m_NormalIndex == m_Normal.size());
        }

    private:
        std::vector<UniformDraw> m_Uniform;
        std::vector<float>       m_Normal;
        std::size_t              m_UniformIndex = 0;
        std::size_t              m_NormalIndex  = 0;
    };

    SHR::RhythmInput RestingInput(float delta)
    {
        return {
            .DeltaSeconds        = delta,
            .HeartRate           = 60.0F,
            .RespirationPhase    = 0.0F,
            .ExertionFraction    = 1.0F,
            .Contractility       = 0.5F,
            .PVCChancePerSecond  = 0.0F,
            .RiskFactor          = 0.0F,
            .RunExtensionChance  = 0.0F,
        };
    }
}

TEST_CASE("Sinus BeatEvent carries timing, filling, and sampled vigor", "[rhythm][characterization]")
{
    ScriptedRandom random(
        {
            { 0.0F, 1.0F, 0.5F },
            { 0.0F, 1.0F, 0.5F },
        },
        { 0.25F }
    );
    SHR::RhythmEngine rhythm(random.Callbacks());
    rhythm.Init();

    REQUIRE_FALSE(rhythm.Advance(RestingInput(0.4F)).has_value());
    const std::optional<SHR::BeatEvent> event = rhythm.Advance(RestingInput(0.6F));

    REQUIRE(event.has_value());
    CHECK(event->Kind == SHR::BeatKind::Sinus);
    CHECK_THAT(event->IBI, Catch::Matchers::WithinAbs(1.0F, Tolerance));
    CHECK_THAT(event->FillingInterval, Catch::Matchers::WithinAbs(1.0F, Tolerance));
    CHECK_THAT(event->CouplingFraction, Catch::Matchers::WithinAbs(0.0F, Tolerance));
    CHECK_THAT(event->Vigor, Catch::Matchers::WithinAbs(0.52125F, Tolerance));
    random.RequireConsumed();
}

TEST_CASE("Sinus timing applies RSA and carries frame overshoot", "[rhythm][characterization]")
{
    ScriptedRandom random(
        {
            { 0.0F, 1.0F, 0.5F },
            { 0.0F, 1.0F, 0.5F },
        },
        { 0.0F, 0.0F }
    );
    SHR::RhythmEngine rhythm(random.Callbacks());
    rhythm.Init();

    SHR::RhythmInput input = RestingInput(1.0F);
    input.RespirationPhase = 0.25F;
    input.ExertionFraction = 0.0F;

    const auto first = rhythm.Advance(input);
    REQUIRE(first.has_value());
    CHECK_THAT(first->IBI, Catch::Matchers::WithinAbs(0.95F, Tolerance));

    input.DeltaSeconds = 0.9F;
    const auto second = rhythm.Advance(input);
    REQUIRE(second.has_value());
    CHECK_THAT(second->IBI, Catch::Matchers::WithinAbs(0.95F, Tolerance));
    random.RequireConsumed();
}

TEST_CASE("Scripted PVC draw produces the extraction-baseline event", "[rhythm][characterization]")
{
    ScriptedRandom random({
        { 0.0F, 1.0F, 0.0F },
        { 1.0F - C::PVCCouplingVariation, 1.0F + C::PVCCouplingVariation, 1.0F },
        { 0.0F, 1.0F, 0.5F },
        { 1.0F - C::PVCPauseVariation, 1.0F + C::PVCPauseVariation, 1.0F },
    });
    SHR::RhythmEngine rhythm(random.Callbacks());
    rhythm.Init();

    SHR::RhythmInput input = RestingInput(0.1F);
    input.PVCChancePerSecond = 1.0F;
    input.RiskFactor = 0.5F;

    REQUIRE_FALSE(rhythm.Advance(input).has_value());
    input.DeltaSeconds = 0.635F;
    const auto event = rhythm.Advance(input);

    REQUIRE(event.has_value());
    CHECK(event->Kind == SHR::BeatKind::PVC);
    CHECK_THAT(event->IBI, Catch::Matchers::WithinAbs(0.735F, Tolerance));
    CHECK_THAT(event->FillingInterval, Catch::Matchers::WithinAbs(0.735F, Tolerance));
    CHECK_THAT(event->CouplingFraction, Catch::Matchers::WithinAbs(0.735F, Tolerance));
    CHECK_THAT(event->Vigor, Catch::Matchers::WithinAbs(0.5F, Tolerance));
    random.RequireConsumed();
}

TEST_CASE("Maximum PVC run consumes scripted draws once and retains in-run timing", "[rhythm][characterization]")
{
    std::vector<UniformDraw> draws{
        { 0.0F, 1.0F, 0.0F },
        { 1.0F - C::PVCCouplingVariation, 1.0F + C::PVCCouplingVariation, 1.0F },
    };
    for (int extension = 0; extension < C::PVCRunMaxLength - 1; ++extension)
    {
        draws.push_back({ 0.0F, 1.0F, 0.0F });
    }
    draws.push_back({
        1.0F - C::PVCPauseVariation,
        1.0F + C::PVCPauseVariation,
        1.0F,
    });

    ScriptedRandom random(std::move(draws));
    SHR::RhythmEngine rhythm(random.Callbacks());
    rhythm.Init();

    SHR::RhythmInput input = RestingInput(C::PVCCouplingMin);
    input.PVCChancePerSecond = 1.0F;
    input.RiskFactor = 1.0F;
    input.RunExtensionChance = 1.0F;

    for (int beat = 0; beat < C::PVCRunMaxLength; ++beat)
    {
        const auto event = rhythm.Advance(input);
        REQUIRE(event.has_value());
        CHECK(event->Kind == SHR::BeatKind::PVC);
        CHECK_THAT(event->IBI, Catch::Matchers::WithinAbs(C::PVCCouplingMin, Tolerance));
        CHECK_THAT(event->FillingInterval, Catch::Matchers::WithinAbs(C::PVCCouplingMin, Tolerance));
        CHECK_THAT(event->CouplingFraction, Catch::Matchers::WithinAbs(C::PVCCouplingMin, Tolerance));
        CHECK_THAT(event->Vigor, Catch::Matchers::WithinAbs(0.5F, Tolerance));
    }
    random.RequireConsumed();
}

TEST_CASE("Post-pause sinus retains the legacy filling proxy and extra IBI", "[rhythm][characterization]")
{
    ScriptedRandom random(
        {
            { 0.0F, 1.0F, 0.0F },
            { 1.0F - C::PVCCouplingVariation, 1.0F + C::PVCCouplingVariation, 1.0F },
            { 0.0F, 1.0F, 0.5F },
            { 1.0F - C::PVCPauseVariation, 1.0F + C::PVCPauseVariation, 1.0F },
        },
        { 0.0F }
    );
    SHR::RhythmEngine rhythm(random.Callbacks());
    rhythm.Init();

    SHR::RhythmInput input = RestingInput(0.735F);
    input.PVCChancePerSecond = 1.0F;
    input.RiskFactor = 0.5F;
    const auto pvc = rhythm.Advance(input);
    REQUIRE(pvc.has_value());
    REQUIRE(pvc->Kind == SHR::BeatKind::PVC);

    input.PVCChancePerSecond = 0.0F;
    input.DeltaSeconds = 1.3F;
    REQUIRE_FALSE(rhythm.Advance(input).has_value());

    input.DeltaSeconds = 0.966F;
    const auto returningSinus = rhythm.Advance(input);

    REQUIRE(returningSinus.has_value());
    CHECK(returningSinus->Kind == SHR::BeatKind::Sinus);
    CHECK_THAT(returningSinus->IBI, Catch::Matchers::WithinAbs(1.0F, Tolerance));
    CHECK_THAT(returningSinus->FillingInterval, Catch::Matchers::WithinAbs(1.265F, Tolerance));
    CHECK_THAT(returningSinus->CouplingFraction, Catch::Matchers::WithinAbs(0.0F, Tolerance));
    CHECK_THAT(returningSinus->Vigor, Catch::Matchers::WithinAbs(0.5F, Tolerance));
    random.RequireConsumed();
}

TEST_CASE("Extreme positive vigor draw is capped in the event", "[rhythm][characterization]")
{
    ScriptedRandom random(
        {
            { 0.0F, 1.0F, 0.5F },
        },
        { 10.0F }
    );
    SHR::RhythmEngine rhythm(random.Callbacks());
    rhythm.Init();

    SHR::RhythmInput input = RestingInput(1.0F);
    input.Contractility = 0.8F;
    const auto event = rhythm.Advance(input);
    constexpr float expectedVigor = 0.8F * (1.0F + C::VigorJitterScale * C::VigorJitterMaxSigma);

    REQUIRE(event.has_value());
    CHECK_THAT(event->Vigor, Catch::Matchers::WithinAbs(expectedVigor, Tolerance));
    random.RequireConsumed();
}
