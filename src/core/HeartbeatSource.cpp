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
#include "core/HeartbeatSource.hpp"

#include "core/Constants.hpp"

#include <pocketfft_hdronly.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace
{
    namespace C = SHR::Constants;

    void RequireMatchingFormats(const SHR::HeartbeatSourceSlices &source)
    {
        if (source.S1.GetFormat() != source.S2.GetFormat())
        {
            throw std::invalid_argument("heartbeat source slices must have the same audio format");
        }
    }

    void ApplyHighPass(SHR::AudioBufferView input, float cutoffHz)
    {
        if (!std::isfinite(cutoffHz))
        {
            throw std::invalid_argument("source high-pass cutoff must be finite");
        }
        if (cutoffHz <= 0.0F || input.Empty()) return;
        if (cutoffHz >= static_cast<float>(input.SampleRate()) / 2.0F)
        {
            throw std::invalid_argument("source high-pass cutoff must be below Nyquist");
        }

        constexpr float q     = 1.0F / std::numbers::sqrt2_v<float>;
        const float     w0    = 2.0F * std::numbers::pi_v<float> * cutoffHz / static_cast<float>(input.SampleRate());
        const float     cw    = std::cos(w0);
        const float     alpha = std::sin(w0) / (2.0F * q);
        const float     a0    = 1.0F + alpha;
        const float     b0    = (1.0F + cw) / 2.0F / a0;
        const float     b1    = -(1.0F + cw)      / a0;
        const float     b2    = (1.0F + cw) / 2.0F / a0;
        const float     a1    = (-2.0F * cw)      / a0;
        const float     a2    = (1.0F - alpha)    / a0;

        struct FilterState
        {
            float X1 = 0.0F;
            float X2 = 0.0F;
            float Y1 = 0.0F;
            float Y2 = 0.0F;
        };
        std::vector<FilterState> states(input.ChannelCount());

        for (std::size_t frame = 0; frame < input.FrameCount(); ++frame)
        {
            for (std::uint32_t channel = 0; channel < input.ChannelCount(); ++channel)
            {
                FilterState &state = states[channel];
                const float  x = input(frame, channel);
                const float  y =
                    b0 * x +
                    b1 * state.X1 +
                    b2 * state.X2 -
                    a1 * state.Y1 -
                    a2 * state.Y2;
                state.X2 = state.X1;
                state.X1 = x;
                state.Y2 = state.Y1;
                state.Y1 = y;
                input(frame, channel) = y;
            }
        }
    }

    std::vector<float> AnalyticEnvelope(SHR::ConstAudioBufferView input)
    {
        const std::size_t frameCount = input.FrameCount();
        if (frameCount == 0) return { };

        // Baseline attack selection deliberately follows channel zero, matching the frozen renderer and
        // NumPy mirror. Combining rectified channels would not produce an analytic signal.
        std::vector<std::complex<double>> buffer(frameCount);
        for (std::size_t frame = 0; frame < frameCount; ++frame)
        {
            buffer[frame] = std::complex<double>{ input(frame, 0), 0.0 };
        }

        const pocketfft::shape_t  shape{ frameCount };
        const pocketfft::stride_t stride{ static_cast<std::ptrdiff_t>(sizeof(std::complex<double>)) };
        const pocketfft::shape_t  axes{ 0 };
        pocketfft::c2c(
            shape,
            stride,
            stride,
            axes,
            pocketfft::FORWARD,
            buffer.data(),
            buffer.data(),
            1.0
        );

        const std::size_t midpoint = frameCount / 2;
        const std::size_t lastPositive = (frameCount % 2 == 0) ? midpoint - 1 : midpoint;
        for (std::size_t i = 1; i <= lastPositive; ++i) buffer[i] *= 2.0;
        for (std::size_t i = midpoint + 1; i < frameCount; ++i) buffer[i] = 0.0;

        pocketfft::c2c(
            shape,
            stride,
            stride,
            axes,
            pocketfft::BACKWARD,
            buffer.data(),
            buffer.data(),
            1.0 / static_cast<double>(frameCount)
        );

        std::vector<float> envelope(frameCount);
        for (std::size_t frame = 0; frame < frameCount; ++frame)
        {
            envelope[frame] = static_cast<float>(std::abs(buffer[frame]));
        }
        return envelope;
    }
}

SHR::HeartbeatSourceSlices SHR::SliceHeartbeatSource(ConstAudioBufferView decoded)
{
    if (decoded.FrameCount() < C::S2EndFrames)
    {
        throw std::invalid_argument("heartbeat source is too short for the S1/S2 landmarks");
    }

    const ConstAudioBufferView s1 = decoded.Subview(
        C::S1OnsetFrames,
        C::S1EndFrames - C::S1OnsetFrames
    );
    const ConstAudioBufferView s2 = decoded.Subview(
        C::S2OnsetFrames,
        C::S2EndFrames - C::S2OnsetFrames
    );
    return {
        .S1 = AudioBuffer(
            s1.GetFormat(),
            s1.FrameCount(),
            std::vector<float>(s1.Samples().begin(), s1.Samples().end())
        ),
        .S2 = AudioBuffer(
            s2.GetFormat(),
            s2.FrameCount(),
            std::vector<float>(s2.Samples().begin(), s2.Samples().end())
        ),
    };
}

void SHR::ApplyHeartbeatSourceHighPass(
    HeartbeatSourceSlices &source,
    float                  cutoffHz
)
{
    RequireMatchingFormats(source);
    ApplyHighPass(source.S1.View(), cutoffHz);
    ApplyHighPass(source.S2.View(), cutoffHz);
}

void SHR::NormalizeHeartbeatSourceJoint(
    HeartbeatSourceSlices &source,
    float                  targetPeak
)
{
    RequireMatchingFormats(source);
    if (!std::isfinite(targetPeak) || targetPeak < 0.0F)
    {
        throw std::invalid_argument("heartbeat source normalization target must be finite and nonnegative");
    }

    float peak = 0.0F;
    for (const float sample : source.S1.ConstView().Samples())
    {
        if (!std::isfinite(sample))
        {
            throw std::invalid_argument("cannot normalize a non-finite heartbeat source sample");
        }
        peak = std::max(peak, std::abs(sample));
    }
    for (const float sample : source.S2.ConstView().Samples())
    {
        if (!std::isfinite(sample))
        {
            throw std::invalid_argument("cannot normalize a non-finite heartbeat source sample");
        }
        peak = std::max(peak, std::abs(sample));
    }
    if (peak == 0.0F) return;

    const float scale = targetPeak / peak;
    for (float &sample : source.S1.View().Samples()) sample *= scale;
    for (float &sample : source.S2.View().Samples()) sample *= scale;
}

std::optional<SHR::BaselineAttackRegion> SHR::FindBaselineAttackRegion(
    ConstAudioBufferView input,
    float                thresholdFraction
)
{
    if (
        !std::isfinite(thresholdFraction) ||
        thresholdFraction < 0.0F ||
        thresholdFraction > 1.0F
    )
    {
        throw std::invalid_argument("attack threshold fraction must be between zero and one");
    }
    if (input.Empty()) return std::nullopt;

    const std::vector<float> envelope = AnalyticEnvelope(input);
    const auto peak = std::max_element(envelope.begin(), envelope.end());
    if (peak == envelope.end() || *peak <= 0.0F) return std::nullopt;

    const std::size_t peakFrame = static_cast<std::size_t>(
        std::distance(envelope.begin(), peak)
    );
    if (peakFrame == 0) return std::nullopt;

    const float threshold = thresholdFraction * *peak;
    std::size_t startFrame = 0;
    for (std::size_t frame = peakFrame; frame-- > 0; )
    {
        if (envelope[frame] < threshold)
        {
            startFrame = frame + 1;
            break;
        }
    }
    return BaselineAttackRegion{
        .StartFrame = startFrame,
        .PeakFrame = peakFrame,
    };
}

SHR::HeartbeatSource SHR::PrepareHeartbeatSource(ConstAudioBufferView decoded)
{
    return PrepareHeartbeatSource(decoded, DefaultModelCoefficients().SourceConditioning);
}

SHR::HeartbeatSource SHR::PrepareHeartbeatSource(
    ConstAudioBufferView                  decoded,
    const SourceConditioningCoefficients &coefficients
)
{
    HeartbeatSourceSlices source = SliceHeartbeatSource(decoded);
    ApplyHeartbeatSourceHighPass(source, coefficients.SourceHighPassHz);
    NormalizeHeartbeatSourceJoint(source, coefficients.SourceRestLevel);
    const std::optional<BaselineAttackRegion> attack = FindBaselineAttackRegion(
        source.S1.ConstView(),
        coefficients.AttackBuildThreshold
    );
    return {
        .S1 = std::move(source.S1),
        .S2 = std::move(source.S2),
        .S1BaselineAttack = attack,
    };
}
