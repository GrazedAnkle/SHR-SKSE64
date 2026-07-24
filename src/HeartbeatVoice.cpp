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
#include "HeartbeatVoice.hpp"

#include "Config.hpp"
#include "Constants.hpp"

#include <pocketfft_hdronly.h>

namespace
{
    namespace C = SHR::Constants;
    constexpr const char WavPath[] = "Data\\Sound\\fx\\SHR_HeartBeat\\HeartBeat_Shortened.wav";

    std::int32_t PeakSample(const std::vector<std::byte> &data)
    {
        std::int32_t peak = 0;
        for (std::size_t i = 0; i + sizeof(std::int16_t) <= data.size(); i += sizeof(std::int16_t))
        {
            std::int16_t s = 0;
            std::memcpy(&s, data.data() + i, sizeof(s));
            peak = std::max(peak, std::abs(static_cast<std::int32_t>(s)));
        }
        return peak;
    }

    void ApplyScale(std::vector<std::byte> &data, float scale)
    {
        for (std::size_t i = 0; i + sizeof(std::int16_t) <= data.size(); i += sizeof(std::int16_t))
        {
            std::int16_t s = 0;
            std::memcpy(&s, data.data() + i, sizeof(s));
            s = static_cast<std::int16_t>(std::clamp(static_cast<float>(s) * scale, -32768.0F, 32767.0F));
            std::memcpy(data.data() + i, &s, sizeof(s));
        }
    }

    // One-pole low-pass over a sub-sample buffer, in place, per channel:
    //   y[n] = y[n-1] + alpha * (x[n] - y[n-1]),  alpha = 1 - exp(-2*pi*fc/fs)
    void ApplyLowPass(
        std::vector<std::byte> &data,
        std::uint32_t           samplesPerFrame,
        float                   cutoffHz,
        float                   sampleRate
    )
    {
        if (cutoffHz <= 0.0F || sampleRate <= 0.0F) return;
        const float alpha = 1.0F - std::exp(-2.0F * std::numbers::pi_v<float> * cutoffHz / sampleRate);
        if (alpha >= 1.0F) return; // exponential underflow makes the recurrence a passthrough

        std::array<float, 2> state = { };
        const std::uint32_t  ch = std::min<std::uint32_t>(samplesPerFrame, 2U);
        const std::size_t    n  = data.size() / sizeof(std::int16_t);

        for (std::size_t i = 0; i < n; ++i)
        {
            std::int16_t s = 0;
            std::memcpy(&s, data.data() + i * sizeof(s), sizeof(s));
            float &y = state[i % ch];
            y += alpha * (static_cast<float>(s) - y);
            const std::int16_t out = static_cast<std::int16_t>(
                std::clamp(y, -32768.0F, 32767.0F)
            );
            std::memcpy(data.data() + i * sizeof(out), &out, sizeof(out));
        }
    }

    // Two-pole RBJ Butterworth high-pass, in place per channel.
    // Filter state remains in float to match tools/engine_offline.py.
    void ApplyHighPass(
        std::vector<std::byte> &data,
        std::uint32_t           samplesPerFrame,
        float                   cutoffHz,
        float                   sampleRate
    )
    {
        if (cutoffHz <= 0.0F || sampleRate <= 0.0F || samplesPerFrame == 0) return;
        constexpr float q     = 1.0F / std::numbers::sqrt2_v<float>;
        const float     w0    = 2.0F * std::numbers::pi_v<float> * cutoffHz / sampleRate;
        const float     cw    = std::cos(w0);
        const float     alpha = std::sin(w0) / (2.0F * q);
        const float     a0    = 1.0F + alpha;
        const float     b0    = (1.0F + cw) / 2.0F / a0;
        const float     b1    = -(1.0F + cw)      / a0;
        const float     b2    = (1.0F + cw) / 2.0F / a0;
        const float     a1    = (-2.0F * cw)      / a0;
        const float     a2    = (1.0F - alpha)    / a0;

        const std::uint32_t  ch = std::min<std::uint32_t>(samplesPerFrame, 2U);
        std::array<float, 2> x1 = { };
        std::array<float, 2> x2 = { };
        std::array<float, 2> y1 = { };
        std::array<float, 2> y2 = { };
        const std::size_t    n  = data.size() / sizeof(std::int16_t);
        for (std::size_t i = 0; i < n; ++i)
        {
            std::int16_t s = 0;
            std::memcpy(&s, data.data() + i * sizeof(s), sizeof(s));
            const float         x  = static_cast<float>(s);
            const std::uint32_t c  = static_cast<std::uint32_t>(i % ch);
            const float         yn = b0 * x + b1 * x1[c] + b2 * x2[c] - a1 * y1[c] - a2 * y2[c];
            x2[c] = x1[c];
            x1[c] = x;
            y2[c] = y1[c];
            y1[c] = yn;
            const std::int16_t out = static_cast<std::int16_t>(std::clamp(yn, -32768.0F, 32767.0F));
            std::memcpy(data.data() + i * sizeof(out), &out, sizeof(out));
        }
    }

    // FFT analytic envelope matching shrlib.env_analytic at the same transform length.
    // Unlike raw |x|, it has no half-cycle zeroes to interrupt the onset walk.
    std::vector<float> AnalyticEnv(const std::vector<float> &x)
    {
        const std::size_t n = x.size();
        if (n == 0) return { };

        std::vector<std::complex<double>> buf(n);
        for (std::size_t i = 0; i < n; ++i) buf[i] = std::complex<double>{ x[i], 0.0 };

        const pocketfft::shape_t  shape{ n };
        const pocketfft::stride_t stride{ static_cast<std::ptrdiff_t>(sizeof(std::complex<double>)) };
        const pocketfft::shape_t  axes{ 0 };
        pocketfft::c2c(shape, stride, stride, axes, pocketfft::FORWARD, buf.data(), buf.data(), 1.0);

        // Keep DC and an even-length Nyquist bin unchanged, double the other positive bins and clear
        // the negative bins. Matches scipy.signal.hilbert.
        const std::size_t mid = n / 2;
        const std::size_t lastPositive = (n % 2 == 0) ? mid - 1 : mid;  // last bin to double
        for (std::size_t i = 1; i <= lastPositive; ++i) buf[i] *= 2.0;
        for (std::size_t i = mid + 1; i < n; ++i) buf[i] = 0.0;

        pocketfft::c2c(
            shape,
            stride,
            stride,
            axes,
            pocketfft::BACKWARD,
            buf.data(),
            buf.data(),
            1.0 / static_cast<double>(n)
        );

        std::vector<float> env(n, 0.0F);
        for (std::size_t i = 0; i < n; ++i) env[i] = static_cast<float>(std::abs(buf[i]));
        return env;
    }

    // Compress the final pre-peak ascent by k without changing the lead-in or post-peak body.
    // Uses the analytic-envelope onset definition from tools/shrlib.py and shortens the buffer.
    void CompressOnsetBuild(std::vector<std::byte> &data, std::uint32_t samplesPerFrame, float k)
    {
        if (k <= 1.0F || samplesPerFrame == 0) return;
        const std::size_t   stride = static_cast<std::size_t>(samplesPerFrame) * sizeof(std::int16_t);
        const std::uint32_t frames = static_cast<std::uint32_t>(data.size() / stride);
        if (frames < 4) return;

        // Channel 0 matches the offline renderer. A rectified inter-channel maximum is not analytic.
        std::vector<float> ch0(frames, 0.0F);
        for (std::uint32_t f = 0; f < frames; ++f)
        {
            std::int16_t v = 0;
            std::memcpy(&v, data.data() + f * stride, sizeof(v));
            ch0[f] = static_cast<float>(v);
        }
        const std::vector<float> env = AnalyticEnv(ch0);

        std::uint32_t peakFrame = 0;
        float         best      = -1.0F;
        for (std::uint32_t f = 0; f < frames; ++f)
        {
            if (env[f] > best) { best = env[f]; peakFrame = f; }
        }
        if (peakFrame == 0 || best <= 0.0F) return;

        // Last pre-peak threshold crossing, matching tools/shrlib.py:onset_peak_idx.
        const float   thr   = C::AttackBuildThreshold * best;
        std::uint32_t onset = 0;
        for (std::uint32_t f = peakFrame; f-- > 0; )
        {
            if (env[f] < thr) { onset = f + 1; break; }
        }

        const std::uint32_t buildLen    = peakFrame - onset;
        const std::uint32_t newBuildLen = std::max(1U, static_cast<std::uint32_t>(static_cast<float>(buildLen) / k));
        if (newBuildLen >= buildLen) return;

        // Rebuild: [0..onset) lead-in + resampled build [onset..peak) -> newBuildLen + [peak..end) body.
        std::vector<std::byte> out;
        out.reserve(data.size() - static_cast<std::size_t>(buildLen - newBuildLen) * stride);
        out.insert(out.end(), data.begin(), data.begin() + static_cast<std::ptrdiff_t>(onset) * static_cast<std::ptrdiff_t>(stride));
        for (std::uint32_t i = 0; i < newBuildLen; ++i)
        {
            const float         srcPos = static_cast<float>(onset) +
                static_cast<float>(i) * static_cast<float>(buildLen) / static_cast<float>(newBuildLen);
            const std::uint32_t i0     = std::min(static_cast<std::uint32_t>(srcPos), peakFrame);
            const std::uint32_t i1     = std::min(i0 + 1U, peakFrame);
            const float         frac   = srcPos - static_cast<float>(i0);
            for (std::uint32_t s = 0; s < samplesPerFrame; ++s)
            {
                std::int16_t a0 = 0;
                std::int16_t a1 = 0;
                std::memcpy(&a0, data.data() + i0 * stride + s * sizeof(a0), sizeof(a0));
                std::memcpy(&a1, data.data() + i1 * stride + s * sizeof(a1), sizeof(a1));
                const std::int16_t v = static_cast<std::int16_t>(
                    std::lerp(static_cast<float>(a0), static_cast<float>(a1), frac)
                );
                const auto *vb = reinterpret_cast<const std::byte *>(&v);
                out.insert(out.end(), vb, vb + sizeof(v));
            }
        }
        out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(peakFrame) * static_cast<std::ptrdiff_t>(stride), data.end());
        data.swap(out);
    }

    // Scale both buffers together, preserving relative amplitude, until their joint peak is
    // 32767 * targetFraction.
    void NormalizeJoint(
        std::vector<std::byte> &a,
        std::vector<std::byte> &b,
        float targetFraction
    )
    {
        const std::int32_t peak = std::max(PeakSample(a), PeakSample(b));
        if (peak == 0) return;
        const float scale = 32767.0F * targetFraction / static_cast<float>(peak);
        ApplyScale(a, scale);
        ApplyScale(b, scale);
    }
}

bool SHR::HeartbeatVoice::LoadWav()
{
    std::ifstream file(WavPath, std::ios::binary | std::ios::ate);
    if (!file)
    {
        SKSE::log::error("[HeartbeatVoice] Cannot open {}", WavPath);
        return false;
    }

    const auto fileSize = static_cast<std::size_t>(file.tellg());
    file.seekg(0);

    std::vector<std::byte> raw(fileSize);
    if (!file.read(reinterpret_cast<char *>(raw.data()), fileSize))
    {
        SKSE::log::error("[HeartbeatVoice] Read failed for {}", WavPath);
        return false;
    }

    if (fileSize < 12 ||
        std::memcmp(raw.data(),     "RIFF", 4) != 0 ||
        std::memcmp(raw.data() + 8, "WAVE", 4) != 0)
    {
        SKSE::log::error("[HeartbeatVoice] Not a valid RIFF/WAVE file");
        return false;
    }

    bool fmtFound = false, dataFound = false;
    std::vector<std::byte> pcmData;

    for (std::size_t pos = 12; pos + 8 <= fileSize; )
    {
        const auto *id = reinterpret_cast<const char *>(raw.data() + pos);
        std::uint32_t chunkSize = 0;
        std::memcpy(&chunkSize, raw.data() + pos + 4, sizeof(chunkSize));

        if (std::memcmp(id, "fmt ", 4) == 0 && chunkSize >= 16)
        {
            // Standard PCM fmt chunks are 16 bytes (no cbSize field); WAVEFORMATEX
            // is 18. Zero-init first so cbSize = 0 when the field is absent.
            m_Format = { };
            std::memcpy(
                &m_Format,
                raw.data() + pos + 8,
                std::min(static_cast<std::size_t>(chunkSize), sizeof(RE::WAVEFORMATEX))
            );
            fmtFound = true;
        }
        else if (std::memcmp(id, "data", 4) == 0)
        {
            const std::size_t dataOffset = pos + 8;
            const std::size_t dataBytes  = std::min(static_cast<std::size_t>(chunkSize), fileSize - dataOffset);
            pcmData.assign(raw.data() + dataOffset, raw.data() + dataOffset + dataBytes);
            dataFound = true;
        }

        pos += 8 + chunkSize + (chunkSize & 1); // skip chunk + word-align pad
    }

    if (!fmtFound || !dataFound)
    {
        SKSE::log::error("[HeartbeatVoice] WAV missing fmt or data chunk");
        return false;
    }

    if (m_Format.wFormatTag != 1 || m_Format.wBitsPerSample != 16)
    {
        SKSE::log::error(
            "[HeartbeatVoice] Only 16-bit PCM WAV is supported (got tag={} bits={})",
            m_Format.wFormatTag,
            m_Format.wBitsPerSample
        );
        return false;
    }

    const std::uint32_t bytesPerFrame = m_Format.nChannels * (m_Format.wBitsPerSample / 8);

    const std::size_t s1Start = static_cast<std::size_t>(C::S1OnsetFrames) * bytesPerFrame;
    const std::size_t s1End   = static_cast<std::size_t>(C::S1EndFrames)   * bytesPerFrame;
    const std::size_t s2Start = static_cast<std::size_t>(C::S2OnsetFrames) * bytesPerFrame;
    const std::size_t s2End   = static_cast<std::size_t>(C::S2EndFrames)   * bytesPerFrame;

    if (s2End > pcmData.size())
    {
        SKSE::log::error(
            "[HeartbeatVoice] WAV too short for S1/S2 landmarks ({} bytes, need {})",
            pcmData.size(),
            s2End
        );
        return false;
    }

    m_S1Data.assign(pcmData.begin() + s1Start, pcmData.begin() + s1End);
    m_S2Data.assign(pcmData.begin() + s2Start, pcmData.begin() + s2End);

    ApplyHighPass(m_S1Data, m_Format.nChannels, C::SourceHighPassHz, static_cast<float>(m_Format.nSamplesPerSec));
    ApplyHighPass(m_S2Data, m_Format.nChannels, C::SourceHighPassHz, static_cast<float>(m_Format.nSamplesPerSec));

    NormalizeJoint(m_S1Data, m_S2Data, C::SourceRestLevel);

    SKSE::log::info(
        "[HeartbeatVoice] WAV loaded: {}Hz {}ch {}bit - S1 {}B S2 {}B",
        m_Format.nSamplesPerSec,
        m_Format.nChannels,
        m_Format.wBitsPerSample,
        m_S1Data.size(),
        m_S2Data.size()
    );
    return true;
}

bool SHR::HeartbeatVoice::Init()
{
    if (!LoadWav()) return false;

    auto *platform = RE::BSAudioManager::QPlatformInstance();
    if (!platform)
    {
        SKSE::log::error("[HeartbeatVoice] QPlatformInstance() returned null");
        return false;
    }

    auto *xa = static_cast<RE::BSXAudio2Audio *>(platform);
    if (!xa->XAudio)
    {
        SKSE::log::error("[HeartbeatVoice] `IXAudio2 *` is null");
        return false;
    }

    // cbSize must be 0 for WAVE_FORMAT_PCM.
    RE::WAVEFORMATEX fmt = m_Format;
    fmt.cbSize = 0;

    const auto hr = xa->XAudio->CreateSourceVoice(
        &m_Voice,
        &fmt,
        0,
        RE::XAUDIO2_DEFAULT_FREQ_RATIO,
        &m_Callback
    );
    if (hr != 0 || !m_Voice)
    {
        SKSE::log::error(
            "[HeartbeatVoice] CreateSourceVoice failed: {:08X}",
            static_cast<std::uint32_t>(hr)
        );
        return false;
    }

    // Voice volume persists across Stop()/Start().
    const float voiceVolume = Config::Get().Audio.Volume * C::VoiceOutputGain;
    m_Voice->SetVolume(voiceVolume);

    m_Voice->Start();
    SKSE::log::info("[HeartbeatVoice] Source voice ready (volume {} -> {})", Config::Get().Audio.Volume, voiceVolume);
    return true;
}

void SHR::HeartbeatVoice::CopySamples(
    std::byte       *dst,
    std::uint32_t    dstFrameOffset,
    const std::byte *src,
    std::uint32_t    srcFrames,
    std::uint32_t    outFrames,
    float            resampleRatio,
    std::uint32_t    bytesPerFrame,
    float            amplitude,
    std::uint32_t    crossfadeFrames,
    bool             fadeIn,
    bool             fadeOut
)
{
    if (outFrames == 0 || srcFrames == 0) return;

    // Guard against overlapping fade-in and fade-out on very short regions.
    const std::uint32_t cf = std::min(crossfadeFrames, outFrames / 2);
    const std::uint32_t samplesPerFrame = bytesPerFrame / sizeof(std::int16_t);

    for (std::uint32_t f = 0; f < outFrames; ++f)
    {
        float taper = 1.0F;
        if (fadeIn  && cf > 0 && f < cf)
        {
            taper *= static_cast<float>(f + 1) / static_cast<float>(cf);
        }
        if (fadeOut && cf > 0 && f >= outFrames - cf)
        {
            taper *= static_cast<float>(outFrames - f) / static_cast<float>(cf);
        }

        const float scale = amplitude * taper;

        // Linear interpolation; ratio > 1 plays faster and shorter, while 1 is a direct copy.
        const float         srcPos = static_cast<float>(f) * resampleRatio;
        const std::uint32_t i0     = std::min(static_cast<std::uint32_t>(srcPos), srcFrames - 1U);
        const std::uint32_t i1     = std::min(i0 + 1U, srcFrames - 1U);
        const float         frac   = srcPos - static_cast<float>(i0);

        const std::size_t off0 = static_cast<std::size_t>(i0) * bytesPerFrame;
        const std::size_t off1 = static_cast<std::size_t>(i1) * bytesPerFrame;
        const std::size_t dstOff = static_cast<std::size_t>(dstFrameOffset + f) * bytesPerFrame;

        for (std::uint32_t s = 0; s < samplesPerFrame; ++s)
        {
            std::int16_t a0 = 0;
            std::int16_t a1 = 0;
            std::memcpy(&a0, src + off0 + s * sizeof(a0), sizeof(a0));
            std::memcpy(&a1, src + off1 + s * sizeof(a1), sizeof(a1));
            const float interp = std::lerp(static_cast<float>(a0), static_cast<float>(a1), frac);

            // MARK: Transducer Stage

            // Apply gain in full-scale units, passing values below the knee unchanged and tanh-limiting
            // only the excess above it.
            float v = interp * scale / 32768.0F;
            const float mag = std::abs(v);
            if (mag > C::SoftClipKnee)
            {
                const float over = (mag - C::SoftClipKnee) / (1.0F - C::SoftClipKnee);
                v = std::copysign(C::SoftClipKnee + (1.0F - C::SoftClipKnee) * std::tanh(over), v);
            }
            const std::int16_t out = static_cast<std::int16_t>(
                std::clamp(v * 32767.0F, -32768.0F, 32767.0F)
            );
            std::memcpy(dst + dstOff + s * sizeof(out), &out, sizeof(out));
        }
    }
}

void SHR::HeartbeatVoice::Play(const RhythmEngine::Beat &beat)
{
    if (!m_Voice || m_S1Data.empty() || m_S2Data.empty()) return;

    const std::uint32_t sampleRate    = m_Format.nSamplesPerSec;
    const std::uint32_t bytesPerFrame = m_Format.nChannels * (m_Format.wBitsPerSample / 8);
    const std::uint32_t crossfadeFrames = static_cast<std::uint32_t>(
        C::CrossfadeMs * 0.001F * static_cast<float>(sampleRate)
    );

    const std::uint32_t totalFrames   = static_cast<std::uint32_t>(
        beat.IBI * static_cast<float>(sampleRate)
    );
    const std::uint32_t systoleFrames = static_cast<std::uint32_t>(
        beat.SystoleDuration * static_cast<float>(sampleRate)
    );

    const std::uint32_t s2FullFrames = static_cast<std::uint32_t>(m_S2Data.size() / bytesPerFrame);

    // Half-sine respiratory envelope over the clamped beat phase.
    const float lungInflation = std::sin(std::numbers::pi_v<float> * std::clamp(beat.RespPhase, 0.0F, 1.0F));
    const float breathDepthFactor = C::BreathDepthRestFraction +
        (1.0F - C::BreathDepthRestFraction) * std::clamp(beat.BreathDepth, 0.0F, 1.0F);
    const float breathAmpFactor = 1.0F - C::BreathAmpDepth * breathDepthFactor * lungInflation;  // attenuation only

    // Per-beat vigor jitter can exceed the mean [0, 1] range.
    const float contractility = std::clamp(beat.Contractility, 0.0F, 3.0F);

    // Shared [0, 1] transmission driver for pitch dip and low-pass cutoff.
    const float breathMuffle = std::clamp(breathDepthFactor * lungInflation, 0.0F, 1.0F);

    // Breath resampling affects both lobes; PVC resampling affects S1 only.
    const float pitchDip = 1.0F - C::BreathPitchDipDepth * breathMuffle;
    const float s1ResampleRatio = (beat.IsPVC ? C::ResamplePVCRatio : 1.0F) * pitchDip;
    const float s2ResampleRatio = pitchDip;

    const float breathCutoffHz = std::lerp(C::BreathLowPassOpenHz, C::BreathLowPassMinHz, breathMuffle);

    // MARK: Source Stage

    const std::uint32_t samplesPerFrame = bytesPerFrame / sizeof(std::int16_t);
    std::vector<std::byte> s1Work = m_S1Data;
    std::vector<std::byte> s2Work = m_S2Data;
    if (!beat.IsPVC)
    {
        const float fsNorm = std::clamp(
            (beat.FrankStarling - C::FrankStarlingMin) / (C::FrankStarlingMax - C::FrankStarlingMin),
            0.0F,
            1.0F
        );
        const float k = 1.0F + (C::AttackCompressMax - 1.0F) * contractility * fsNorm;
        CompressOnsetBuild(s1Work, samplesPerFrame, k);
    }

    // MARK: Transmission Stage

    // Filtering in source time means resampling shifts the realized output-domain corner. See WI-008.
    for (int pole = 0; pole < C::BreathLowPassPoles; ++pole)
    {
        ApplyLowPass(s1Work, samplesPerFrame, breathCutoffHz, static_cast<float>(sampleRate));
        ApplyLowPass(s2Work, samplesPerFrame, breathCutoffHz, static_cast<float>(sampleRate));
    }

    // Onset compression can change the S1 frame count.
    const std::uint32_t s1FullFrames = static_cast<std::uint32_t>(s1Work.size() / bytesPerFrame);

    const std::uint32_t s1Resampled = static_cast<std::uint32_t>(static_cast<float>(s1FullFrames) / s1ResampleRatio);
    const std::uint32_t s2Resampled = static_cast<std::uint32_t>(static_cast<float>(s2FullFrames) / s2ResampleRatio);

    // Keep both copies within their available beat windows and guard the S2 subtraction against underflow.
    const std::uint32_t s1Cap    = static_cast<std::uint32_t>(C::S1SystoleFraction * static_cast<float>(systoleFrames));
    const std::uint32_t s1Frames = std::min(s1Resampled, s1Cap);

    const std::uint32_t s2StartFrame = systoleFrames;

    const std::uint32_t s2Window     = (totalFrames > s2StartFrame) ? (totalFrames - s2StartFrame) : 0U;
    const std::uint32_t s2Cap        = static_cast<std::uint32_t>(C::S2WindowFraction * static_cast<float>(s2Window));
    const std::uint32_t s2CopyFrames = std::min({ s2Resampled, s2Cap, s2Window });

    // pContext owns the buffer until BeatBufferCallback::OnBufferEnd.
    auto *heapBuf = new std::vector<std::byte>(
        static_cast<std::size_t>(totalFrames) * bytesPerFrame,
        std::byte{ 0 }
    );

    CopySamples(
        heapBuf->data(),
        0,
        s1Work.data(),
        s1FullFrames,
        s1Frames,
        s1ResampleRatio,
        bytesPerFrame,
        beat.S1Amplitude * breathAmpFactor,
        crossfadeFrames,
        false,  // no fade-in
        true    // fade out
    );

    if (s2CopyFrames > 0)
    {
        CopySamples(
            heapBuf->data(),
            s2StartFrame,
            s2Work.data(),
            s2FullFrames,
            s2CopyFrames,
            s2ResampleRatio,
            bytesPerFrame,
            beat.S2Amplitude * breathAmpFactor,
            crossfadeFrames,
            true,  // fade in
            true   // fade out
        );
    }

    RE::XAUDIO2_BUFFER buf = { };
    buf.AudioBytes = static_cast<std::uint32_t>(heapBuf->size());
    buf.pAudioData = heapBuf->data();
    buf.pContext = heapBuf;
    m_Voice->SubmitSourceBuffer(&buf);
}

void SHR::HeartbeatVoice::Pause()
{
    if (!m_Voice || m_IsPaused) return;
    m_Voice->Stop();
    m_IsPaused = true;
}

void SHR::HeartbeatVoice::Resume()
{
    if (!m_Voice || !m_IsPaused) return;
    m_Voice->Start();
    m_IsPaused = false;
}

void SHR::HeartbeatVoice::FlushAndStop()
{
    if (!m_Voice) return;
    m_Voice->Stop();
    m_Voice->FlushSourceBuffers();
    m_Voice->Start();
}

void SHR::HeartbeatVoice::Shutdown()
{
    if (!m_Voice) return;
    m_Voice->Stop();
    m_Voice->FlushSourceBuffers();
    m_Voice->DestroyVoice();
    m_Voice = nullptr;
    m_S1Data.clear();
    m_S2Data.clear();
}
