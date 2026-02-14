#pragma once

#include "sfz/types.h"
#include "core/config.h"

#include <vector>
#include <cmath>

namespace sfz
{
    // Fixed LFO (amplfo, fillfo, pitchlfo).
    // Simple single-waveform oscillator with delay and fade-in.
    class FixedLFO
    {
    public:
        FixedLFO() = default;

        void configure(const FixedLFOParams& params, SampleRate sampleRate);
        void noteOn();
        float process();

        float getValue() const { return m_output; }

    private:
        float       m_freq          = 0.0f;
        float       m_depth         = 0.0f;
        float       m_phase         = 0.0f;
        float       m_phaseInc      = 0.0f;
        float       m_output        = 0.0f;
        float       m_fadeGain      = 0.0f;
        float       m_fadeRate      = 0.0f;
        uint32_t    m_delaySamples  = 0;
        uint32_t    m_sampleCounter = 0;
        SampleRate  m_sampleRate    = 44100;
    };

    // Flex LFO (SFZ v2 lfoN).
    // Supports multiple waveforms, step sequencer, phase control, and
    // modulation to arbitrary targets.
    class FlexLFO
    {
    public:
        FlexLFO() = default;

        void configure(const FlexLFOParams& params, SampleRate sampleRate);
        void noteOn();
        float process();

        float getValue() const { return m_output; }

        // Get the modulated output scaled by a target depth
        float getTargetValue(float depth) const { return m_output * depth; }

        bool isFinished() const { return m_finished; }

    private:
        // Generate raw waveform at current phase
        float generateWave(LfoWave wave, float phase) const;

        LfoWave     m_wave          = LfoWave::Triangle;
        float       m_freq          = 0.0f;
        float       m_phase         = 0.0f;
        float       m_initialPhase  = 0.0f;
        float       m_phaseInc      = 0.0f;
        float       m_output        = 0.0f;
        float       m_smooth        = 0.0f;
        float       m_prevOutput    = 0.0f;
        float       m_fadeGain      = 0.0f;
        float       m_fadeRate      = 0.0f;
        uint32_t    m_delaySamples  = 0;
        uint32_t    m_sampleCounter = 0;
        int         m_count         = 0;    // 0 = infinite
        int         m_cyclesDone    = 0;
        bool        m_finished      = false;
        SampleRate  m_sampleRate    = 44100;

        // Step sequencer
        int                     m_steps = 0;
        std::vector<float>      m_stepValues;

        // Target depths (copied from FlexLFOParams)
        FlexLFOTarget           m_targets;
    };

} // namespace sfz
