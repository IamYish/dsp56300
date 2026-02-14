#pragma once

#include "sfz/types.h"
#include "dsp/filter.h"
#include "core/config.h"

#include <array>

namespace sfz
{
    // 3-band parametric EQ, as defined by the SFZ specification.
    // Each band can be peak, low shelf, or high shelf.
    class ParametricEQ
    {
    public:
        ParametricEQ() = default;

        // Configure all 3 bands from region parameters.
        void configure(const std::array<EQBandParams, Config::MaxEQBands>& bands,
                       float velocity, SampleRate sampleRate);

        // Update a single band's parameters (for real-time modulation).
        void updateBand(int band, float freqHz, float bwOctaves, float gainDb);

        // Process a single sample through all 3 bands (serial).
        float process(float input);

        // Reset all filter states.
        void reset();

        // Check if any band is active.
        bool isActive() const;

    private:
        struct Band
        {
            BiquadFilter    filter;
            float           freq    = 0.0f;
            float           bw      = 1.0f;
            float           gain    = 0.0f;
            EqType          type    = EqType::Peak;
            bool            active  = false;
        };

        std::array<Band, Config::MaxEQBands>    m_bands;
        SampleRate                              m_sampleRate = 44100;
    };

} // namespace sfz
