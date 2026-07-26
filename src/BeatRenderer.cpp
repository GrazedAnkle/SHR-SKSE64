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
#include "BeatRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
    SHR::AudioBuffer CopyBuffer(SHR::ConstAudioBufferView input)
    {
        return SHR::AudioBuffer(
            input.GetFormat(),
            input.FrameCount(),
            std::vector<float>(input.Samples().begin(), input.Samples().end())
        );
    }

    void ValidateSource(const SHR::HeartbeatSource &source)
    {
        if (
            source.S1.GetFormat() != source.S2.GetFormat() ||
            source.S1.Empty() ||
            source.S2.Empty()
        )
        {
            throw std::invalid_argument("heartbeat source is not renderable");
        }
        if (
            source.S1BaselineAttack &&
            (
                source.S1BaselineAttack->StartFrame > source.S1BaselineAttack->PeakFrame ||
                source.S1BaselineAttack->PeakFrame >= source.S1.FrameCount()
            )
        )
        {
            throw std::invalid_argument("baseline S1 attack region is outside the source");
        }
    }

    void ValidateRender(const SHR::RenderSpec &render)
    {
        const std::array values{
            render.IBI,
            render.SystoleDuration,
            render.S1Amplitude,
            render.S2Amplitude,
            render.S1ResampleRatio,
            render.S2ResampleRatio,
            render.LowPassCutoffHz,
            render.OnsetCompression,
        };
        if (!std::ranges::all_of(values, [](float value) { return std::isfinite(value); }))
        {
            throw std::invalid_argument("beat render specification must be finite");
        }
        if (
            render.IBI <= 0.0F ||
            render.SystoleDuration < 0.0F ||
            render.S1Amplitude < 0.0F ||
            render.S2Amplitude < 0.0F ||
            render.S1ResampleRatio <= 0.0F ||
            render.S2ResampleRatio <= 0.0F ||
            render.OnsetCompression < 0.0F
        )
        {
            throw std::invalid_argument("beat render specification is outside its valid range");
        }
        if (render.Kind != SHR::BeatKind::Sinus && render.Kind != SHR::BeatKind::PVC)
        {
            throw std::invalid_argument("beat render specification has an unknown beat kind");
        }
    }

    std::uint32_t FrameCountFromSeconds(float seconds, std::uint32_t sampleRate)
    {
        const float frames = seconds * static_cast<float>(sampleRate);
        if (
            frames < 0.0F ||
            frames >= static_cast<float>(std::numeric_limits<std::uint32_t>::max())
        )
        {
            throw std::length_error("rendered heartbeat frame count exceeds uint32_t");
        }
        return static_cast<std::uint32_t>(frames);
    }

    std::uint32_t ResampledFrameCount(std::size_t sourceFrames, float ratio)
    {
        const float frames = static_cast<float>(sourceFrames) / ratio;
        if (
            !std::isfinite(frames) ||
            frames >= static_cast<float>(std::numeric_limits<std::uint32_t>::max())
        )
        {
            throw std::length_error("resampled heartbeat frame count exceeds uint32_t");
        }
        return static_cast<std::uint32_t>(frames);
    }

    SHR::AudioBuffer PrepareS1SourceStage(
        const SHR::HeartbeatSource &source,
        const SHR::RenderSpec      &render
    )
    {
        const SHR::ConstAudioBufferView input = source.S1.ConstView();
        if (
            render.Kind == SHR::BeatKind::PVC ||
            render.OnsetCompression <= 1.0F ||
            !source.S1BaselineAttack
        )
        {
            return CopyBuffer(input);
        }

        const std::size_t onsetFrame = source.S1BaselineAttack->StartFrame;
        const std::size_t peakFrame = source.S1BaselineAttack->PeakFrame;
        const std::size_t buildFrames = peakFrame - onsetFrame;
        const std::size_t compressedFrames = std::max<std::size_t>(
            1,
            static_cast<std::size_t>(
                static_cast<float>(buildFrames) / render.OnsetCompression
            )
        );
        if (compressedFrames >= buildFrames) return CopyBuffer(input);

        SHR::AudioBuffer output(
            input.GetFormat(),
            input.FrameCount() - buildFrames + compressedFrames
        );
        for (std::size_t frame = 0; frame < onsetFrame; ++frame)
        {
            for (std::uint32_t channel = 0; channel < input.ChannelCount(); ++channel)
            {
                output.View()(frame, channel) = input(frame, channel);
            }
        }

        for (std::size_t frame = 0; frame < compressedFrames; ++frame)
        {
            const float sourcePosition = static_cast<float>(onsetFrame) +
                static_cast<float>(frame) * static_cast<float>(buildFrames) /
                static_cast<float>(compressedFrames);
            const std::size_t i0 = std::min(
                static_cast<std::size_t>(sourcePosition),
                peakFrame
            );
            const std::size_t i1 = std::min(i0 + 1, peakFrame);
            const float fraction = sourcePosition - static_cast<float>(i0);
            for (std::uint32_t channel = 0; channel < input.ChannelCount(); ++channel)
            {
                output.View()(onsetFrame + frame, channel) = std::lerp(
                    input(i0, channel),
                    input(i1, channel),
                    fraction
                );
            }
        }

        const std::size_t suffixOffset = onsetFrame + compressedFrames;
        for (std::size_t frame = peakFrame; frame < input.FrameCount(); ++frame)
        {
            for (std::uint32_t channel = 0; channel < input.ChannelCount(); ++channel)
            {
                output.View()(suffixOffset + frame - peakFrame, channel) =
                    input(frame, channel);
            }
        }
        return output;
    }

    void ApplyLowPass(SHR::AudioBufferView input, float cutoffHz)
    {
        if (cutoffHz <= 0.0F || input.Empty()) return;
        const float alpha = 1.0F - std::exp(
            -2.0F * std::numbers::pi_v<float> * cutoffHz /
            static_cast<float>(input.SampleRate())
        );
        if (alpha >= 1.0F) return;

        std::vector<float> states(input.ChannelCount(), 0.0F);
        for (std::size_t frame = 0; frame < input.FrameCount(); ++frame)
        {
            for (std::uint32_t channel = 0; channel < input.ChannelCount(); ++channel)
            {
                float &state = states[channel];
                state += alpha * (input(frame, channel) - state);
                input(frame, channel) = state;
            }
        }
    }

    void ApplyTransmission(
        SHR::AudioBuffer                     &s1,
        SHR::AudioBuffer                     &s2,
        float                                 cutoffHz,
        const SHR::BeatRenderingCoefficients &coefficients
    )
    {
        // Filtering in source time means resampling shifts the realized output-domain corner. See WI-008.
        for (int pole = 0; pole < coefficients.BreathLowPassPoles; ++pole)
        {
            ApplyLowPass(s1.View(), cutoffHz);
            ApplyLowPass(s2.View(), cutoffHz);
        }
    }

    void CopyResampled(
        SHR::AudioBufferView      destination,
        std::size_t               destinationFrameOffset,
        SHR::ConstAudioBufferView source,
        std::size_t               outputFrames,
        float                     resampleRatio,
        float                     amplitude,
        std::size_t               crossfadeFrames,
        bool                      fadeIn,
        bool                      fadeOut
    )
    {
        if (outputFrames == 0 || source.Empty()) return;

        const std::size_t crossfade = std::min(crossfadeFrames, outputFrames / 2);
        for (std::size_t frame = 0; frame < outputFrames; ++frame)
        {
            float taper = 1.0F;
            if (fadeIn && crossfade > 0 && frame < crossfade)
            {
                taper *= static_cast<float>(frame + 1) / static_cast<float>(crossfade);
            }
            if (fadeOut && crossfade > 0 && frame >= outputFrames - crossfade)
            {
                taper *= static_cast<float>(outputFrames - frame) /
                    static_cast<float>(crossfade);
            }

            const float sourcePosition = static_cast<float>(frame) * resampleRatio;
            const std::size_t i0 = std::min(
                static_cast<std::size_t>(sourcePosition),
                source.FrameCount() - 1
            );
            const std::size_t i1 = std::min(i0 + 1, source.FrameCount() - 1);
            const float fraction = sourcePosition - static_cast<float>(i0);
            const float scale = amplitude * taper;
            for (std::uint32_t channel = 0; channel < source.ChannelCount(); ++channel)
            {
                destination(destinationFrameOffset + frame, channel) = std::lerp(
                    source(i0, channel),
                    source(i1, channel),
                    fraction
                ) * scale;
            }
        }
    }

    SHR::AudioBuffer MixTransducerInput(
        SHR::ConstAudioBufferView             s1,
        SHR::ConstAudioBufferView             s2,
        const SHR::RenderSpec                &render,
        const SHR::BeatRenderingCoefficients &coefficients
    )
    {
        const std::uint32_t sampleRate = s1.SampleRate();
        const std::uint32_t totalFrames = FrameCountFromSeconds(render.IBI, sampleRate);
        const std::uint32_t systoleFrames = FrameCountFromSeconds(
            render.SystoleDuration,
            sampleRate
        );
        const std::uint32_t crossfadeFrames = FrameCountFromSeconds(
            coefficients.CrossfadeMs * 0.001F,
            sampleRate
        );

        const std::uint32_t s1Resampled = ResampledFrameCount(
            s1.FrameCount(),
            render.S1ResampleRatio
        );
        const std::uint32_t s2Resampled = ResampledFrameCount(
            s2.FrameCount(),
            render.S2ResampleRatio
        );
        const std::uint32_t s1Cap = static_cast<std::uint32_t>(
            coefficients.S1SystoleFraction * static_cast<float>(systoleFrames)
        );
        const std::uint32_t s1Frames = std::min({ s1Resampled, s1Cap, totalFrames });
        const std::uint32_t s2StartFrame = systoleFrames;
        const std::uint32_t s2Window = (totalFrames > s2StartFrame)
            ? totalFrames - s2StartFrame
            : 0U;
        const std::uint32_t s2Cap = static_cast<std::uint32_t>(
            coefficients.S2WindowFraction * static_cast<float>(s2Window)
        );
        const std::uint32_t s2Frames = std::min({ s2Resampled, s2Cap, s2Window });

        SHR::AudioBuffer output(s1.GetFormat(), totalFrames);
        CopyResampled(
            output.View(),
            0,
            s1,
            s1Frames,
            render.S1ResampleRatio,
            render.S1Amplitude,
            crossfadeFrames,
            false,
            true
        );
        CopyResampled(
            output.View(),
            s2StartFrame,
            s2,
            s2Frames,
            render.S2ResampleRatio,
            render.S2Amplitude,
            crossfadeFrames,
            true,
            true
        );
        return output;
    }

    void ApplySoftKnee(
        SHR::AudioBufferView                  input,
        const SHR::BeatRenderingCoefficients &coefficients
    )
    {
        for (float &sample : input.Samples())
        {
            const float magnitude = std::abs(sample);
            if (magnitude <= coefficients.SoftClipKnee) continue;

            const float over = (magnitude - coefficients.SoftClipKnee) /
                (1.0F - coefficients.SoftClipKnee);
            sample = std::copysign(
                coefficients.SoftClipKnee +
                    (1.0F - coefficients.SoftClipKnee) * std::tanh(over),
                sample
            );
        }
    }
}

SHR::AudioBuffer SHR::RenderBeat(
    const HeartbeatSource &source,
    const RenderSpec      &render
)
{
    return RenderBeat(source, render, DefaultModelCoefficients().BeatRendering);
}

SHR::AudioBuffer SHR::RenderBeat(
    const HeartbeatSource           &source,
    const RenderSpec                &render,
    const BeatRenderingCoefficients &coefficients
)
{
    ValidateSource(source);
    ValidateRender(render);

    AudioBuffer sourceS1 = PrepareS1SourceStage(source, render);
    AudioBuffer sourceS2 = CopyBuffer(source.S2.ConstView());
    ApplyTransmission(sourceS1, sourceS2, render.LowPassCutoffHz, coefficients);
    AudioBuffer output = MixTransducerInput(
        sourceS1.ConstView(),
        sourceS2.ConstView(),
        render,
        coefficients
    );
    ApplySoftKnee(output.View(), coefficients);
    return output;
}

SHR::BeatRenderTrace SHR::TraceBeatRender(
    const HeartbeatSource &source,
    const RenderSpec      &render
)
{
    return TraceBeatRender(source, render, DefaultModelCoefficients().BeatRendering);
}

SHR::BeatRenderTrace SHR::TraceBeatRender(
    const HeartbeatSource           &source,
    const RenderSpec                &render,
    const BeatRenderingCoefficients &coefficients
)
{
    ValidateSource(source);
    ValidateRender(render);

    AudioBuffer sourceS1 = PrepareS1SourceStage(source, render);
    AudioBuffer sourceS2 = CopyBuffer(source.S2.ConstView());
    AudioBuffer transmittedS1 = sourceS1;
    AudioBuffer transmittedS2 = sourceS2;
    ApplyTransmission(
        transmittedS1,
        transmittedS2,
        render.LowPassCutoffHz,
        coefficients
    );
    AudioBuffer transducerInput = MixTransducerInput(
        transmittedS1.ConstView(),
        transmittedS2.ConstView(),
        render,
        coefficients
    );
    AudioBuffer output = transducerInput;
    ApplySoftKnee(output.View(), coefficients);
    return {
        .SourceS1 = std::move(sourceS1),
        .SourceS2 = std::move(sourceS2),
        .TransmittedS1 = std::move(transmittedS1),
        .TransmittedS2 = std::move(transmittedS2),
        .TransducerInput = std::move(transducerInput),
        .Output = std::move(output),
    };
}
