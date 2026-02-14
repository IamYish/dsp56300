#pragma once

#include "sfz/types.h"

#include <cmath>
#include <array>

namespace sfz
{
    // Multi-mode biquad filter supporting all SFZ v2 filter types.
    // Implements a transposed direct-form II biquad filter.
    // Supports 1-pole, 2-pole, 4-pole, and 6-pole configurations
    // by cascading biquad sections.
    class BiquadFilter
    {
    public:
        BiquadFilter() = default;

        // Configure for a specific filter type, cutoff, and resonance.
        void configure(FilterType type, float cutoffHz, float resonanceDb,
                       SampleRate sampleRate);

        // Update cutoff and resonance without changing type.
        void updateParameters(float cutoffHz, float resonanceDb);

        // Process a single sample.
        float process(float input);

        // Reset filter state (clear delay lines).
        void reset();

    private:
        // Biquad coefficients
        struct Coeffs
        {
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
            float a1 = 0.0f, a2 = 0.0f;
        };

        // Biquad state (transposed direct-form II)
        struct State
        {
            float z1 = 0.0f, z2 = 0.0f;
        };

        void computeCoefficients();

        // Process one biquad section
        float processBiquad(float input, const Coeffs& c, State& s) const
        {
            const float out = c.b0 * input + s.z1;
            s.z1 = c.b1 * input - c.a1 * out + s.z2;
            s.z2 = c.b2 * input - c.a2 * out;
            return out;
        }

        FilterType  m_type          = FilterType::None;
        float       m_cutoff        = 20000.0f;
        float       m_resonance     = 0.0f;
        float       m_sampleRate    = 44100.0f;

        // Up to 3 cascaded biquad sections (for 6-pole)
        static constexpr int MaxSections = 3;
        std::array<Coeffs, MaxSections> m_coeffs;
        std::array<State, MaxSections>  m_state;
        int         m_numSections   = 0;
    };

    // Comb filter for the 'comb' filter type.
    class CombFilter
    {
    public:
        CombFilter() = default;

        void configure(float cutoffHz, float feedback, SampleRate sampleRate);
        float process(float input);
        void reset();

    private:
        std::array<float, 48000>    m_buffer{};     // max 1 second at 48kHz
        int                         m_writePos  = 0;
        int                         m_delaySamples = 0;
        float                       m_feedback  = 0.0f;
    };

} // namespace sfz
