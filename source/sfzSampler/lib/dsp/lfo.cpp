#include "lfo.h"

#include <algorithm>

namespace sfz
{
    static constexpr float TwoPi = 6.283185307179586f;

    // ---- Fixed LFO ----

    void FixedLFO::configure(const FixedLFOParams& params, SampleRate sampleRate)
    {
        m_sampleRate = sampleRate;
        m_freq = params.freq.base;
        m_depth = params.depth.base;
        m_delaySamples = static_cast<uint32_t>(params.delay.base * static_cast<float>(sampleRate));

        const float fadeTime = params.fade.base;
        m_fadeRate = (fadeTime > 0.0f) ? (1.0f / (fadeTime * static_cast<float>(sampleRate))) : 1.0f;

        m_phaseInc = m_freq / static_cast<float>(sampleRate);
    }

    void FixedLFO::noteOn()
    {
        m_phase = 0.0f;
        m_output = 0.0f;
        m_fadeGain = (m_fadeRate >= 1.0f) ? 1.0f : 0.0f;
        m_sampleCounter = 0;
    }

    float FixedLFO::process()
    {
        // Delay phase
        if (m_sampleCounter < m_delaySamples)
        {
            ++m_sampleCounter;
            m_output = 0.0f;
            return 0.0f;
        }

        // Fade in
        if (m_fadeGain < 1.0f)
        {
            m_fadeGain = std::min(1.0f, m_fadeGain + m_fadeRate);
        }

        // Triangle wave (default for fixed LFOs)
        // Phase goes 0->1, triangle output goes -1 to +1
        float rawValue = 4.0f * std::abs(m_phase - 0.5f) - 1.0f;

        m_output = rawValue * m_depth * m_fadeGain;

        // Advance phase
        m_phase += m_phaseInc;
        if (m_phase >= 1.0f)
            m_phase -= 1.0f;

        return m_output;
    }

    // ---- Flex LFO ----

    void FlexLFO::configure(const FlexLFOParams& params, SampleRate sampleRate)
    {
        m_sampleRate = sampleRate;
        m_wave = params.wave;
        m_freq = params.freq.base;
        m_initialPhase = params.phase.base;
        m_smooth = params.smooth.base;
        m_count = params.count;
        m_steps = params.steps;
        m_stepValues = params.stepValues;
        m_targets = params.targets;

        m_delaySamples = static_cast<uint32_t>(
            std::max(0.0f, params.delay.base) * static_cast<float>(sampleRate));

        const float fadeTime = params.fade.base;
        m_fadeRate = (fadeTime > 0.0f) ? (1.0f / (fadeTime * static_cast<float>(sampleRate))) : 1.0f;

        m_phaseInc = m_freq / static_cast<float>(sampleRate);
    }

    void FlexLFO::noteOn()
    {
        m_phase = m_initialPhase;
        m_output = 0.0f;
        m_prevOutput = 0.0f;
        m_fadeGain = (m_fadeRate >= 1.0f) ? 1.0f : 0.0f;
        m_sampleCounter = 0;
        m_cyclesDone = 0;
        m_finished = false;
    }

    float FlexLFO::process()
    {
        if (m_finished)
            return m_output;

        if (m_sampleCounter < m_delaySamples)
        {
            ++m_sampleCounter;
            m_output = 0.0f;
            return 0.0f;
        }

        // Fade in
        if (m_fadeGain < 1.0f)
            m_fadeGain = std::min(1.0f, m_fadeGain + m_fadeRate);

        float rawValue;

        if (m_steps > 0 && !m_stepValues.empty())
        {
            // Step sequencer mode
            int stepIndex = static_cast<int>(m_phase * static_cast<float>(m_steps))
                            % static_cast<int>(m_stepValues.size());
            if (stepIndex < 0) stepIndex = 0;
            rawValue = m_stepValues[stepIndex];
        }
        else
        {
            rawValue = generateWave(m_wave, m_phase);
        }

        rawValue *= m_fadeGain;

        // Apply smoothing
        if (m_smooth > 0.0f)
        {
            const float coeff = std::exp(-TwoPi / (m_smooth * 0.001f * static_cast<float>(m_sampleRate)));
            rawValue = m_prevOutput + (1.0f - coeff) * (rawValue - m_prevOutput);
            m_prevOutput = rawValue;
        }

        m_output = rawValue;

        // Advance phase
        float prevPhase = m_phase;
        m_phase += m_phaseInc;
        if (m_phase >= 1.0f)
        {
            m_phase -= 1.0f;
            ++m_cyclesDone;
            if (m_count > 0 && m_cyclesDone >= m_count)
            {
                m_finished = true;
                m_output = 0.0f;
            }
        }

        return m_output;
    }

    float FlexLFO::generateWave(LfoWave wave, float phase) const
    {
        switch (wave)
        {
        case LfoWave::Triangle:
            return 4.0f * std::abs(phase - 0.5f) - 1.0f;

        case LfoWave::Sine:
            return std::sin(phase * TwoPi);

        case LfoWave::Pulse75:
            return (phase < 0.75f) ? 1.0f : -1.0f;

        case LfoWave::Square:
            return (phase < 0.5f) ? 1.0f : -1.0f;

        case LfoWave::Pulse25:
            return (phase < 0.25f) ? 1.0f : -1.0f;

        case LfoWave::Pulse12:
            return (phase < 0.125f) ? 1.0f : -1.0f;

        case LfoWave::SawUp:
            return 2.0f * phase - 1.0f;

        case LfoWave::SawDown:
            return 1.0f - 2.0f * phase;

        default:
            return 0.0f;
        }
    }

} // namespace sfz
