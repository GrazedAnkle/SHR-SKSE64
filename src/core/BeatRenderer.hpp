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

#include "core/AudioBuffer.hpp"
#include "core/HeartbeatSource.hpp"
#include "core/RenderSpec.hpp"

namespace SHR
{
    // Diagnostic value for migration tests and future offline inspection. Normal playback consumes only
    // RenderBeat's final output and does not pay for these retained intermediate copies.
    struct BeatRenderTrace
    {
        AudioBuffer SourceS1;
        AudioBuffer SourceS2;
        AudioBuffer TransmittedS1;
        AudioBuffer TransmittedS2;
        AudioBuffer TransducerInput;
        AudioBuffer Output;
    };

    AudioBuffer RenderBeat(
        const HeartbeatSource &source,
        const RenderSpec      &render
    );
    AudioBuffer RenderBeat(
        const HeartbeatSource           &source,
        const RenderSpec                &render,
        const BeatRenderingCoefficients &coefficients
    );

    // Offline counterfactual entry point: continue from core-owned, post-onset-compression source
    // stages. Python may alter those stages for a retired-effect audition, while transmission,
    // resampling/mixing, and limiting remain the compiled production implementation.
    AudioBuffer RenderBeatFromSourceStages(
        ConstAudioBufferView sourceS1,
        ConstAudioBufferView sourceS2,
        const RenderSpec    &render
    );
    AudioBuffer RenderBeatFromSourceStages(
        ConstAudioBufferView             sourceS1,
        ConstAudioBufferView             sourceS2,
        const RenderSpec                &render,
        const BeatRenderingCoefficients &coefficients
    );

    BeatRenderTrace TraceBeatRender(
        const HeartbeatSource &source,
        const RenderSpec      &render
    );
    BeatRenderTrace TraceBeatRender(
        const HeartbeatSource           &source,
        const RenderSpec                &render,
        const BeatRenderingCoefficients &coefficients
    );
}
