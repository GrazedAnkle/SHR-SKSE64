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

#include "core/Constants.hpp"
#include "core/ModelCoefficientRegistry.hpp"

namespace SHR
{
    struct SimulationModelCoefficients
    {
#define SHR_DECLARE_MODEL_COEFFICIENT(type, name) type name = Constants::name;
        SHR_SIMULATION_MODEL_COEFFICIENTS(SHR_DECLARE_MODEL_COEFFICIENT)
#undef SHR_DECLARE_MODEL_COEFFICIENT

        [[nodiscard]] constexpr float FitnessAbsoluteMin() const
        {
            return FitnessBaseMets + (BaseRestingHR - MaxRestingHR) / RestingHRSlope;
        }

        friend constexpr bool operator==(
            const SimulationModelCoefficients &,
            const SimulationModelCoefficients &
        ) = default;
    };

    struct RhythmModelCoefficients
    {
#define SHR_DECLARE_MODEL_COEFFICIENT(type, name) type name = Constants::name;
        SHR_RHYTHM_MODEL_COEFFICIENTS(SHR_DECLARE_MODEL_COEFFICIENT)
#undef SHR_DECLARE_MODEL_COEFFICIENT

        friend constexpr bool operator==(
            const RhythmModelCoefficients &,
            const RhythmModelCoefficients &
        ) = default;
    };

    struct AcousticMappingCoefficients
    {
#define SHR_DECLARE_MODEL_COEFFICIENT(type, name) type name = Constants::name;
        SHR_ACOUSTIC_MAPPING_COEFFICIENTS(SHR_DECLARE_MODEL_COEFFICIENT)
#undef SHR_DECLARE_MODEL_COEFFICIENT

        friend constexpr bool operator==(
            const AcousticMappingCoefficients &,
            const AcousticMappingCoefficients &
        ) = default;
    };

    struct SourceConditioningCoefficients
    {
#define SHR_DECLARE_MODEL_COEFFICIENT(type, name) type name = Constants::name;
        SHR_SOURCE_CONDITIONING_COEFFICIENTS(SHR_DECLARE_MODEL_COEFFICIENT)
#undef SHR_DECLARE_MODEL_COEFFICIENT

        friend constexpr bool operator==(
            const SourceConditioningCoefficients &,
            const SourceConditioningCoefficients &
        ) = default;
    };

    struct BeatRenderingCoefficients
    {
#define SHR_DECLARE_MODEL_COEFFICIENT(type, name) type name = Constants::name;
        SHR_BEAT_RENDERING_COEFFICIENTS(SHR_DECLARE_MODEL_COEFFICIENT)
#undef SHR_DECLARE_MODEL_COEFFICIENT

        friend constexpr bool operator==(
            const BeatRenderingCoefficients &,
            const BeatRenderingCoefficients &
        ) = default;
    };

    class ModelCoefficients
    {
    public:
        ModelCoefficients();
        ModelCoefficients(
            SimulationModelCoefficients    simulation,
            RhythmModelCoefficients        rhythm,
            AcousticMappingCoefficients    acousticMapping,
            SourceConditioningCoefficients sourceConditioning,
            BeatRenderingCoefficients      beatRendering
        );

        const SimulationModelCoefficients    Simulation;
        const RhythmModelCoefficients        Rhythm;
        const AcousticMappingCoefficients    AcousticMapping;
        const SourceConditioningCoefficients SourceConditioning;
        const BeatRenderingCoefficients      BeatRendering;

        friend constexpr bool operator==(
            const ModelCoefficients &,
            const ModelCoefficients &
        ) = default;
    };

    // The immutable production default. Offline callers construct replacements; live owners never mutate it.
    const ModelCoefficients &DefaultModelCoefficients();
}
