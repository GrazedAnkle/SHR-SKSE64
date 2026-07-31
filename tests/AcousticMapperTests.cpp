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
#include "core/AcousticMapper.hpp"

#include "core/Constants.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

namespace
{
    namespace C = SHR::Constants;
    constexpr float Tolerance = 1.0e-5F;

    SHR::PhysiologySnapshot BaselinePhysiology()
    {
        return {
            .HeartRate           = 60.0F,
            .FastHeartRate       = 60.0F,
            .SlowHeartRate       = 60.0F,
            .Exertion            = 1.0F,
            .Adrenaline          = 0.0F,
            .Contractility       = 0.5F,
            .ContractilityExcess = 0.2F,
            .Fitness             = 10.0F,
            .EffectiveFitness    = 10.0F,
            .AcuteFatigue        = 0.0F,
            .LongTermFatigue     = 0.0F,
            .RespirationRate     = 14.0F,
            .RespirationDepth    = 0.4F,
            .RespirationPhase    = 0.0F,
            .DeathSeconds        = std::nullopt,
        };
    }
}

TEST_CASE("Lung inflation curve is core-owned and bounded", "[acoustic][respiration]")
{
    CHECK_THAT(
        SHR::ComputeLungInflation(0.0F),
        Catch::Matchers::WithinAbs(0.0F, Tolerance)
    );
    CHECK_THAT(
        SHR::ComputeLungInflation(0.5F),
        Catch::Matchers::WithinAbs(1.0F, Tolerance)
    );
    CHECK_THAT(
        SHR::ComputeLungInflation(1.0F),
        Catch::Matchers::WithinAbs(0.0F, Tolerance)
    );
    CHECK_THAT(
        SHR::ComputeLungInflation(-1.0F),
        Catch::Matchers::WithinAbs(0.0F, Tolerance)
    );
    CHECK_THAT(
        SHR::ComputeLungInflation(2.0F),
        Catch::Matchers::WithinAbs(0.0F, Tolerance)
    );
}

TEST_CASE("Sinus render mapping matches the extraction baseline", "[acoustic][characterization]")
{
    const SHR::BeatEvent event{
        .IBI              = 1.0F,
        .FillingInterval  = 1.0F,
        .CouplingFraction = 0.0F,
        .Vigor            = 0.52125F,
        .Kind             = SHR::BeatKind::Sinus,
    };

    const SHR::RenderSpec render = SHR::CreateRenderSpec(event, BaselinePhysiology());

    CHECK(render.Kind == SHR::BeatKind::Sinus);
    CHECK_THAT(render.IBI, Catch::Matchers::WithinAbs(1.0F, Tolerance));
    CHECK_THAT(render.SystoleDuration, Catch::Matchers::WithinAbs(0.3424F, Tolerance));
    CHECK_THAT(
        render.S1Amplitude,
        Catch::Matchers::WithinAbs(
            std::pow(10.0F, (C::ContractilityGainDb / 20.0F) * event.Vigor),
            Tolerance
        )
    );
    CHECK_THAT(render.S2Amplitude, Catch::Matchers::WithinAbs(1.0F, Tolerance));
    CHECK_THAT(render.S1ResampleRatio, Catch::Matchers::WithinAbs(1.0F, Tolerance));
    CHECK_THAT(render.S2ResampleRatio, Catch::Matchers::WithinAbs(1.0F, Tolerance));
    CHECK_THAT(render.LowPassCutoffHz, Catch::Matchers::WithinAbs(450.0F, Tolerance));
    CHECK_THAT(render.OnsetCompression, Catch::Matchers::WithinAbs(1.3909375F, Tolerance));
}

TEST_CASE("PVC render mapping matches the extraction baseline", "[acoustic][characterization]")
{
    const SHR::BeatEvent event{
        .IBI              = 0.735F,
        .FillingInterval  = 0.735F,
        .CouplingFraction = 0.735F,
        .Vigor            = 0.5F,
        .Kind             = SHR::BeatKind::PVC,
    };

    const SHR::RenderSpec render = SHR::CreateRenderSpec(event, BaselinePhysiology());

    CHECK(render.Kind == SHR::BeatKind::PVC);
    CHECK_THAT(render.IBI, Catch::Matchers::WithinAbs(0.735F, Tolerance));
    CHECK_THAT(render.SystoleDuration, Catch::Matchers::WithinAbs(0.19932F, Tolerance));
    CHECK_THAT(render.S1Amplitude, Catch::Matchers::WithinAbs(0.55125F, Tolerance));
    CHECK_THAT(render.S2Amplitude, Catch::Matchers::WithinAbs(C::PVCS2Amplitude, Tolerance));
    CHECK_THAT(render.S1ResampleRatio, Catch::Matchers::WithinAbs(C::ResamplePVCRatio, Tolerance));
    CHECK_THAT(render.S2ResampleRatio, Catch::Matchers::WithinAbs(1.0F, Tolerance));
    CHECK_THAT(render.LowPassCutoffHz, Catch::Matchers::WithinAbs(450.0F, Tolerance));
}

TEST_CASE("Peak inspiration maps the complete transmission state", "[acoustic][characterization]")
{
    const SHR::BeatEvent event{
        .IBI              = 1.0F,
        .FillingInterval  = 1.0F,
        .CouplingFraction = 0.0F,
        .Vigor            = 0.0F,
        .Kind             = SHR::BeatKind::Sinus,
    };
    SHR::PhysiologySnapshot physiology = BaselinePhysiology();
    physiology.ContractilityExcess = 0.0F;
    physiology.RespirationDepth = 1.0F;
    physiology.RespirationPhase = 0.5F;

    const SHR::RenderSpec render = SHR::CreateRenderSpec(event, physiology);

    CHECK_THAT(render.SystoleDuration, Catch::Matchers::WithinAbs(0.3624F, Tolerance));
    CHECK_THAT(render.S1Amplitude, Catch::Matchers::WithinAbs(0.53F, Tolerance));
    CHECK_THAT(render.S2Amplitude, Catch::Matchers::WithinAbs(0.53F, Tolerance));
    CHECK_THAT(render.S1ResampleRatio, Catch::Matchers::WithinAbs(0.86F, Tolerance));
    CHECK_THAT(render.S2ResampleRatio, Catch::Matchers::WithinAbs(0.86F, Tolerance));
    CHECK_THAT(render.LowPassCutoffHz, Catch::Matchers::WithinAbs(112.0F, Tolerance));
    CHECK_THAT(render.OnsetCompression, Catch::Matchers::WithinAbs(1.0F, Tolerance));
}

TEST_CASE("Post-pause filling proxy retains its acoustic effect", "[acoustic][characterization]")
{
    const SHR::BeatEvent event{
        .IBI              = 1.0F,
        .FillingInterval  = 1.265F,
        .CouplingFraction = 0.0F,
        .Vigor            = 0.5F,
        .Kind             = SHR::BeatKind::Sinus,
    };

    const SHR::RenderSpec render = SHR::CreateRenderSpec(event, BaselinePhysiology());

    CHECK_THAT(
        render.S1Amplitude,
        Catch::Matchers::WithinAbs(
            1.265F * std::pow(10.0F, (C::ContractilityGainDb / 20.0F) * event.Vigor),
            Tolerance
        )
    );
    CHECK_THAT(render.OnsetCompression, Catch::Matchers::WithinAbs(1.57375F, Tolerance));
}

TEST_CASE("Extreme sampled vigor retains the extraction-baseline acoustic gain", "[acoustic][characterization]")
{
    const SHR::BeatEvent event{
        .IBI              = 1.0F,
        .FillingInterval  = 1.0F,
        .CouplingFraction = 0.0F,
        .Vigor            = 1.14F,
        .Kind             = SHR::BeatKind::Sinus,
    };

    const SHR::RenderSpec render = SHR::CreateRenderSpec(event, BaselinePhysiology());

    CHECK_THAT(
        render.S1Amplitude,
        Catch::Matchers::WithinAbs(
            std::pow(10.0F, (C::ContractilityGainDb / 20.0F) * event.Vigor),
            Tolerance
        )
    );
    CHECK_THAT(render.OnsetCompression, Catch::Matchers::WithinAbs(1.855F, Tolerance));
}
