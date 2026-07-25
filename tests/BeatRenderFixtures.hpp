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

#include "RenderSpec.hpp"

#include <array>
#include <string_view>

namespace SHR::Tests
{
    struct NamedBeatRenderFixture
    {
        std::string_view Name;
        RenderSpec       Render;
    };

    inline constexpr std::array BeatRenderFixtures{
        NamedBeatRenderFixture{
            .Name = "rest",
            .Render = {
                .IBI              = 60.0F / 79.0F,
                .SystoleDuration  = 0.32991F,
                .S1Amplitude      = 1.0F,
                .S2Amplitude      = 1.0F,
                .S1ResampleRatio  = 1.0F,
                .S2ResampleRatio  = 1.0F,
                .LowPassCutoffHz  = 450.0F,
                .OnsetCompression = 1.0F,
                .Kind             = BeatKind::Sinus,
            },
        },
        NamedBeatRenderFixture{
            .Name = "peak",
            .Render = {
                .IBI              = 60.0F / 177.0F,
                .SystoleDuration  = 0.130F,
                .S1Amplitude      = 3.5481339F,
                .S2Amplitude      = 1.0F,
                .S1ResampleRatio  = 1.0F,
                .S2ResampleRatio  = 1.0F,
                .LowPassCutoffHz  = 450.0F,
                .OnsetCompression = 2.5F,
                .Kind             = BeatKind::Sinus,
            },
        },
        NamedBeatRenderFixture{
            .Name = "recovery",
            .Render = {
                .IBI              = 0.6F,
                .SystoleDuration  = 0.244F,
                .S1Amplitude      = 2.25F,
                .S2Amplitude      = 1.0F,
                .S1ResampleRatio  = 1.0F,
                .S2ResampleRatio  = 1.0F,
                .LowPassCutoffHz  = 450.0F,
                .OnsetCompression = 1.8F,
                .Kind             = BeatKind::Sinus,
            },
        },
        NamedBeatRenderFixture{
            .Name = "inspiration",
            .Render = {
                .IBI              = 60.0F / 79.0F,
                .SystoleDuration  = 0.32991F,
                .S1Amplitude      = 0.53F,
                .S2Amplitude      = 0.53F,
                .S1ResampleRatio  = 0.86F,
                .S2ResampleRatio  = 0.86F,
                .LowPassCutoffHz  = 112.0F,
                .OnsetCompression = 1.0F,
                .Kind             = BeatKind::Sinus,
            },
        },
        NamedBeatRenderFixture{
            .Name = "pvc",
            .Render = {
                .IBI              = 0.735F,
                .SystoleDuration  = 0.19932F,
                .S1Amplitude      = 0.55125F,
                .S2Amplitude      = 0.60F,
                .S1ResampleRatio  = 0.90F,
                .S2ResampleRatio  = 1.0F,
                .LowPassCutoffHz  = 450.0F,
                .OnsetCompression = 1.0F,
                .Kind             = BeatKind::PVC,
            },
        },
        // The two edge fixtures below carry provisional values. They guard the renderer's most nonlinear
        // branches, not a validated operating point. Their extreme-end amplitudes are extrapolated (the
        // corpus has no max-effort reference); the extreme-value audit (ROADMAP "maintenance candidates")
        // may retune them, after which the golden manifest is regenerated via
        // tools/check_beat_renderer_golden.py.
        //
        // extreme-vigor: vigor-jitter ceiling. Peak is Vigor 1.0 -> S1Amplitude 10^(0.55*1.0) = 3.548;
        // the VigorJitterMaxSigma(2.5) * VigorJitterScale(0.17) clamp lifts effective vigor to 1.425, so
        // S1Amplitude = 10^(0.55*1.425) ~= 6.079, with OnsetCompression at AttackCompressMax. Timing is
        // borrowed from peak. This is the only fixture that drives ApplySoftKnee hard into saturation.
        NamedBeatRenderFixture{
            .Name = "extreme-vigor",
            .Render = {
                .IBI              = 60.0F / 177.0F,
                .SystoleDuration  = 0.130F,
                .S1Amplitude      = 6.079F,
                .S2Amplitude      = 1.0F,
                .S1ResampleRatio  = 1.0F,
                .S2ResampleRatio  = 1.0F,
                .LowPassCutoffHz  = 450.0F,
                .OnsetCompression = 2.5F,
                .Kind             = BeatKind::Sinus,
            },
        },
        // truncation: synthetic robustness case, not a physiological rate. IBI(0.12) < SystoleDuration
        // drops the S2 window to zero (MixTransducerInput's s2Window==0 path) and forces S1's s1Cap /
        // totalFrames clamp to bind. No other fixture reaches the S2-absent branch.
        NamedBeatRenderFixture{
            .Name = "truncation",
            .Render = {
                .IBI              = 0.12F,
                .SystoleDuration  = 0.130F,
                .S1Amplitude      = 1.0F,
                .S2Amplitude      = 1.0F,
                .S1ResampleRatio  = 1.0F,
                .S2ResampleRatio  = 1.0F,
                .LowPassCutoffHz  = 450.0F,
                .OnsetCompression = 1.0F,
                .Kind             = BeatKind::Sinus,
            },
        },
    };
}
