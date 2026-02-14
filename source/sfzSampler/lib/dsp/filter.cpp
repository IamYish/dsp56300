#include "filter.h"

#include <algorithm>
#include <cmath>

namespace sfz
{
    static constexpr float Pi = 3.14159265358979323846f;

    void BiquadFilter::configure(FilterType type, float cutoffHz, float resonanceDb,
                                 SampleRate sampleRate)
    {
        m_type = type;
        m_cutoff = cutoffHz;
        m_resonance = resonanceDb;
        m_sampleRate = static_cast<float>(sampleRate);

        // Determine number of cascaded sections
        switch (type)
        {
        case FilterType::LPF_1P:
        case FilterType::HPF_1P:
        case FilterType::BPF_1P:
        case FilterType::BRF_1P:
        case FilterType::APF_1P:
            m_numSections = 1;
            break;
        case FilterType::LPF_2P:
        case FilterType::HPF_2P:
        case FilterType::BPF_2P:
        case FilterType::BRF_2P:
        case FilterType::PKF_2P:
        case FilterType::LSH:
        case FilterType::HSH:
        case FilterType::PEQ:
            m_numSections = 1;
            break;
        case FilterType::LPF_4P:
        case FilterType::HPF_4P:
            m_numSections = 2;
            break;
        case FilterType::LPF_6P:
        case FilterType::HPF_6P:
            m_numSections = 3;
            break;
        default:
            m_numSections = 0;
            return;
        }

        computeCoefficients();
    }

    void BiquadFilter::updateParameters(float cutoffHz, float resonanceDb)
    {
        m_cutoff = cutoffHz;
        m_resonance = resonanceDb;
        computeCoefficients();
    }

    float BiquadFilter::process(float input)
    {
        if (m_numSections == 0 || m_type == FilterType::None)
            return input;

        float out = input;
        for (int i = 0; i < m_numSections; ++i)
            out = processBiquad(out, m_coeffs[i], m_state[i]);
        return out;
    }

    void BiquadFilter::reset()
    {
        for (auto& s : m_state)
            s = {};
    }

    void BiquadFilter::computeCoefficients()
    {
        if (m_numSections == 0)
            return;

        const float freq = std::clamp(m_cutoff, 20.0f, m_sampleRate * 0.49f);
        const float w0 = 2.0f * Pi * freq / m_sampleRate;
        const float cosw0 = std::cos(w0);
        const float sinw0 = std::sin(w0);

        // Q from resonance in dB
        const float Q = std::max(0.5f, std::pow(10.0f, m_resonance / 20.0f));
        const float alpha = sinw0 / (2.0f * Q);

        Coeffs c{};
        float a0 = 1.0f;

        switch (m_type)
        {
        // ---- 1-pole filters (implemented as degenerate biquad) ----
        case FilterType::LPF_1P:
        {
            const float rc = 1.0f / (2.0f * Pi * freq);
            const float dt = 1.0f / m_sampleRate;
            const float a = dt / (rc + dt);
            c.b0 = a; c.b1 = 0.0f; c.b2 = 0.0f;
            c.a1 = -(1.0f - a); c.a2 = 0.0f;
            a0 = 1.0f;
            break;
        }
        case FilterType::HPF_1P:
        {
            const float rc = 1.0f / (2.0f * Pi * freq);
            const float dt = 1.0f / m_sampleRate;
            const float a = rc / (rc + dt);
            c.b0 = a; c.b1 = -a; c.b2 = 0.0f;
            c.a1 = -(1.0f - a); c.a2 = 0.0f;
            a0 = 1.0f;
            break;
        }

        // ---- 2-pole filters ----
        case FilterType::LPF_2P:
        case FilterType::LPF_4P:
        case FilterType::LPF_6P:
            c.b0 = (1.0f - cosw0) / 2.0f;
            c.b1 = 1.0f - cosw0;
            c.b2 = (1.0f - cosw0) / 2.0f;
            a0   = 1.0f + alpha;
            c.a1 = -2.0f * cosw0;
            c.a2 = 1.0f - alpha;
            break;

        case FilterType::HPF_2P:
        case FilterType::HPF_4P:
        case FilterType::HPF_6P:
            c.b0 = (1.0f + cosw0) / 2.0f;
            c.b1 = -(1.0f + cosw0);
            c.b2 = (1.0f + cosw0) / 2.0f;
            a0   = 1.0f + alpha;
            c.a1 = -2.0f * cosw0;
            c.a2 = 1.0f - alpha;
            break;

        case FilterType::BPF_1P:
        case FilterType::BPF_2P:
            c.b0 = alpha;
            c.b1 = 0.0f;
            c.b2 = -alpha;
            a0   = 1.0f + alpha;
            c.a1 = -2.0f * cosw0;
            c.a2 = 1.0f - alpha;
            break;

        case FilterType::BRF_1P:
        case FilterType::BRF_2P:
            c.b0 = 1.0f;
            c.b1 = -2.0f * cosw0;
            c.b2 = 1.0f;
            a0   = 1.0f + alpha;
            c.a1 = -2.0f * cosw0;
            c.a2 = 1.0f - alpha;
            break;

        case FilterType::APF_1P:
            c.b0 = 1.0f - alpha;
            c.b1 = -2.0f * cosw0;
            c.b2 = 1.0f + alpha;
            a0   = 1.0f + alpha;
            c.a1 = -2.0f * cosw0;
            c.a2 = 1.0f - alpha;
            break;

        case FilterType::PKF_2P:
        case FilterType::PEQ:
        {
            const float A = std::pow(10.0f, m_resonance / 40.0f);
            c.b0 = 1.0f + alpha * A;
            c.b1 = -2.0f * cosw0;
            c.b2 = 1.0f - alpha * A;
            a0   = 1.0f + alpha / A;
            c.a1 = -2.0f * cosw0;
            c.a2 = 1.0f - alpha / A;
            break;
        }

        case FilterType::LSH:
        {
            const float A = std::pow(10.0f, m_resonance / 40.0f);
            const float sqA = std::sqrt(A);
            c.b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqA * alpha);
            c.b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0);
            c.b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqA * alpha);
            a0   = (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqA * alpha;
            c.a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0);
            c.a2 = (A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqA * alpha;
            break;
        }

        case FilterType::HSH:
        {
            const float A = std::pow(10.0f, m_resonance / 40.0f);
            const float sqA = std::sqrt(A);
            c.b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * sqA * alpha);
            c.b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0);
            c.b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * sqA * alpha);
            a0   = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * sqA * alpha;
            c.a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0);
            c.a2 = (A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * sqA * alpha;
            break;
        }

        default:
            c.b0 = 1.0f; a0 = 1.0f;
            break;
        }

        // Normalize
        if (a0 != 0.0f && a0 != 1.0f)
        {
            c.b0 /= a0;
            c.b1 /= a0;
            c.b2 /= a0;
            c.a1 /= a0;
            c.a2 /= a0;
        }

        // Apply the same coefficients to all cascaded sections
        for (int i = 0; i < m_numSections; ++i)
            m_coeffs[i] = c;
    }

    // ---- Comb Filter ----

    void CombFilter::configure(float cutoffHz, float feedback, SampleRate sampleRate)
    {
        m_feedback = std::clamp(feedback, -0.99f, 0.99f);
        if (cutoffHz > 0.0f)
            m_delaySamples = static_cast<int>(static_cast<float>(sampleRate) / cutoffHz);
        else
            m_delaySamples = 1;
        m_delaySamples = std::clamp(m_delaySamples, 1, static_cast<int>(m_buffer.size() - 1));
    }

    float CombFilter::process(float input)
    {
        const int readPos = (m_writePos - m_delaySamples + static_cast<int>(m_buffer.size()))
                            % static_cast<int>(m_buffer.size());
        const float delayed = m_buffer[readPos];
        const float output = input + delayed * m_feedback;
        m_buffer[m_writePos] = output;
        m_writePos = (m_writePos + 1) % static_cast<int>(m_buffer.size());
        return output;
    }

    void CombFilter::reset()
    {
        m_buffer.fill(0.0f);
        m_writePos = 0;
    }

} // namespace sfz
