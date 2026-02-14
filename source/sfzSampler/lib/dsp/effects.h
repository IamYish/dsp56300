#pragma once

#include "sfz/types.h"

#include <vector>
#include <memory>
#include <array>
#include <cmath>

namespace sfz
{
    // Base class for all effects processors.
    // Effects process stereo audio in-place.
    class Effect
    {
    public:
        virtual ~Effect() = default;

        virtual void configure(const EffectParams& params, SampleRate sampleRate) = 0;
        virtual void process(float* left, float* right, uint32_t numFrames) = 0;
        virtual void reset() = 0;
    };

    // Simple stereo reverb (Freeverb-style).
    class ReverbEffect : public Effect
    {
    public:
        void configure(const EffectParams& params, SampleRate sampleRate) override;
        void process(float* left, float* right, uint32_t numFrames) override;
        void reset() override;

    private:
        struct CombFilter
        {
            std::vector<float>  buffer;
            int                 writePos = 0;
            float               feedback = 0.0f;
            float               damp1    = 0.0f;
            float               damp2    = 0.0f;
            float               filterStore = 0.0f;

            void setSize(int size) { buffer.resize(size, 0.0f); }
            float process(float input)
            {
                float output = buffer[writePos];
                filterStore = output * damp2 + filterStore * damp1;
                buffer[writePos] = input + filterStore * feedback;
                if (++writePos >= static_cast<int>(buffer.size()))
                    writePos = 0;
                return output;
            }
            void clear() { std::fill(buffer.begin(), buffer.end(), 0.0f); filterStore = 0.0f; }
        };

        struct AllpassFilter
        {
            std::vector<float>  buffer;
            int                 writePos = 0;
            float               feedback = 0.5f;

            void setSize(int size) { buffer.resize(size, 0.0f); }
            float process(float input)
            {
                float buffered = buffer[writePos];
                float output = -input + buffered;
                buffer[writePos] = input + buffered * feedback;
                if (++writePos >= static_cast<int>(buffer.size()))
                    writePos = 0;
                return output;
            }
            void clear() { std::fill(buffer.begin(), buffer.end(), 0.0f); }
        };

        static constexpr int NumCombs    = 8;
        static constexpr int NumAllpasses = 4;

        std::array<CombFilter, NumCombs>        m_combL, m_combR;
        std::array<AllpassFilter, NumAllpasses>  m_apL, m_apR;
        float m_wet = 0.3f;
        float m_dry = 0.7f;
        float m_roomSize = 0.7f;
        float m_damp = 0.5f;
    };

    // Simple stereo delay effect.
    class DelayEffect : public Effect
    {
    public:
        void configure(const EffectParams& params, SampleRate sampleRate) override;
        void process(float* left, float* right, uint32_t numFrames) override;
        void reset() override;

    private:
        std::vector<float>  m_bufferL, m_bufferR;
        int                 m_writePos  = 0;
        int                 m_delaySamples = 0;
        float               m_feedback  = 0.3f;
        float               m_wet       = 0.3f;
        float               m_dry       = 0.7f;
    };

    // Effect bus: manages a chain of effects.
    class EffectBus
    {
    public:
        void addEffect(std::unique_ptr<Effect> effect);
        void process(float* left, float* right, uint32_t numFrames);
        void reset();

    private:
        std::vector<std::unique_ptr<Effect>>    m_effects;
    };

    // Create an effect instance from EffectParams.
    std::unique_ptr<Effect> createEffect(const EffectParams& params, SampleRate sampleRate);

} // namespace sfz
