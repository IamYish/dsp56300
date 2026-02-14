#include "effects.h"

#include <algorithm>
#include <cstring>

namespace sfz
{
    // ---- Reverb ----

    // Freeverb comb filter sizes (tuned for 44100 Hz)
    static constexpr int combTuning[] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static constexpr int allpassTuning[] = {556, 441, 341, 225};
    static constexpr int stereospread = 23;

    void ReverbEffect::configure(const EffectParams& params, SampleRate sampleRate)
    {
        const float srScale = static_cast<float>(sampleRate) / 44100.0f;

        // Read effect-specific parameters with defaults
        auto getParam = [&](const std::string& name, float def) -> float {
            auto it = params.params.find(name);
            return (it != params.params.end()) ? it->second.base : def;
        };

        m_roomSize = getParam("reverb_size", 0.7f);
        m_damp     = getParam("reverb_damp", 0.5f);
        m_wet      = getParam("reverb_wet", 0.3f);
        m_dry      = getParam("reverb_dry", 0.7f);

        for (int i = 0; i < NumCombs; ++i)
        {
            int sizeL = static_cast<int>(static_cast<float>(combTuning[i]) * srScale);
            int sizeR = static_cast<int>(static_cast<float>(combTuning[i] + stereospread) * srScale);
            m_combL[i].setSize(sizeL);
            m_combR[i].setSize(sizeR);
            m_combL[i].feedback = m_roomSize;
            m_combR[i].feedback = m_roomSize;
            m_combL[i].damp1 = m_damp;
            m_combL[i].damp2 = 1.0f - m_damp;
            m_combR[i].damp1 = m_damp;
            m_combR[i].damp2 = 1.0f - m_damp;
        }

        for (int i = 0; i < NumAllpasses; ++i)
        {
            int sizeL = static_cast<int>(static_cast<float>(allpassTuning[i]) * srScale);
            int sizeR = static_cast<int>(static_cast<float>(allpassTuning[i] + stereospread) * srScale);
            m_apL[i].setSize(sizeL);
            m_apR[i].setSize(sizeR);
        }
    }

    void ReverbEffect::process(float* left, float* right, uint32_t numFrames)
    {
        for (uint32_t i = 0; i < numFrames; ++i)
        {
            const float inL = left[i];
            const float inR = right[i];
            const float input = (inL + inR) * 0.5f;

            float outL = 0.0f, outR = 0.0f;

            // Parallel comb filters
            for (int c = 0; c < NumCombs; ++c)
            {
                outL += m_combL[c].process(input);
                outR += m_combR[c].process(input);
            }

            // Series allpass filters
            for (int a = 0; a < NumAllpasses; ++a)
            {
                outL = m_apL[a].process(outL);
                outR = m_apR[a].process(outR);
            }

            left[i]  = inL * m_dry + outL * m_wet;
            right[i] = inR * m_dry + outR * m_wet;
        }
    }

    void ReverbEffect::reset()
    {
        for (auto& c : m_combL) c.clear();
        for (auto& c : m_combR) c.clear();
        for (auto& a : m_apL)   a.clear();
        for (auto& a : m_apR)   a.clear();
    }

    // ---- Delay ----

    void DelayEffect::configure(const EffectParams& params, SampleRate sampleRate)
    {
        auto getParam = [&](const std::string& name, float def) -> float {
            auto it = params.params.find(name);
            return (it != params.params.end()) ? it->second.base : def;
        };

        const float delayTimeMs = getParam("delay_time", 250.0f);
        m_feedback = getParam("delay_feedback", 0.3f);
        m_wet      = getParam("delay_wet", 0.3f);
        m_dry      = getParam("delay_dry", 0.7f);

        m_delaySamples = static_cast<int>(delayTimeMs * 0.001f * static_cast<float>(sampleRate));
        const int maxSize = static_cast<int>(sampleRate) * 4; // max 4 seconds
        m_delaySamples = std::clamp(m_delaySamples, 1, maxSize);

        m_bufferL.resize(m_delaySamples + 1, 0.0f);
        m_bufferR.resize(m_delaySamples + 1, 0.0f);
        m_writePos = 0;
    }

    void DelayEffect::process(float* left, float* right, uint32_t numFrames)
    {
        const int bufSize = static_cast<int>(m_bufferL.size());

        for (uint32_t i = 0; i < numFrames; ++i)
        {
            const int readPos = (m_writePos - m_delaySamples + bufSize) % bufSize;

            const float delayedL = m_bufferL[readPos];
            const float delayedR = m_bufferR[readPos];

            m_bufferL[m_writePos] = left[i] + delayedL * m_feedback;
            m_bufferR[m_writePos] = right[i] + delayedR * m_feedback;

            left[i]  = left[i] * m_dry + delayedL * m_wet;
            right[i] = right[i] * m_dry + delayedR * m_wet;

            m_writePos = (m_writePos + 1) % bufSize;
        }
    }

    void DelayEffect::reset()
    {
        std::fill(m_bufferL.begin(), m_bufferL.end(), 0.0f);
        std::fill(m_bufferR.begin(), m_bufferR.end(), 0.0f);
        m_writePos = 0;
    }

    // ---- Effect Bus ----

    void EffectBus::addEffect(std::unique_ptr<Effect> effect)
    {
        m_effects.push_back(std::move(effect));
    }

    void EffectBus::process(float* left, float* right, uint32_t numFrames)
    {
        for (auto& effect : m_effects)
            effect->process(left, right, numFrames);
    }

    void EffectBus::reset()
    {
        for (auto& effect : m_effects)
            effect->reset();
    }

    // ---- Factory ----

    std::unique_ptr<Effect> createEffect(const EffectParams& params, SampleRate sampleRate)
    {
        std::unique_ptr<Effect> effect;

        switch (params.type)
        {
        case EffectType::Reverb:
            effect = std::make_unique<ReverbEffect>();
            break;
        case EffectType::Delay:
            effect = std::make_unique<DelayEffect>();
            break;
        // Other effect types: stub implementations for now
        default:
            return nullptr;
        }

        if (effect)
            effect->configure(params, sampleRate);

        return effect;
    }

} // namespace sfz
