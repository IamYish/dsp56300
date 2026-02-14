#pragma once

#include "sfz/types.h"
#include "core/config.h"

#include <vector>
#include <cmath>

namespace sfz
{
    // DAHDSR envelope generator (for ampeg, fileg, pitcheg).
    // Processes one sample at a time. All times are in seconds.
    class DAHDSREnvelope
    {
    public:
        enum class Stage
        {
            Idle,
            Delay,
            Attack,
            Hold,
            Decay,
            Sustain,
            Release,
            Finished,
        };

        DAHDSREnvelope() = default;

        // Configure the envelope from parsed parameters.
        void configure(const EnvelopeParams& params, float velocity, SampleRate sampleRate);

        // Trigger the envelope (note on).
        void noteOn();

        // Release the envelope (note off).
        void noteOff();

        // Process one sample. Returns the envelope value [0, 1].
        float process();

        // Get current value without advancing.
        float getValue() const { return m_value; }

        // Get current stage.
        Stage getStage() const { return m_stage; }

        // Is the envelope finished (can the voice be freed)?
        bool isFinished() const { return m_stage == Stage::Finished; }

    private:
        void advanceStage();
        float computeRate(float timeSeconds) const;

        Stage       m_stage         = Stage::Idle;
        float       m_value         = 0.0f;
        float       m_sampleRate    = 44100.0f;

        // Resolved parameters (after velocity modulation)
        float       m_delayTime     = 0.0f;
        float       m_startLevel    = 0.0f;
        float       m_attackTime    = 0.0f;
        float       m_holdTime      = 0.0f;
        float       m_decayTime     = 0.0f;
        float       m_sustainLevel  = 1.0f;
        float       m_releaseTime   = 0.0f;

        // Precomputed per-sample increments
        float       m_attackRate    = 0.0f;
        float       m_decayRate     = 0.0f;
        float       m_releaseRate   = 0.0f;

        uint32_t    m_stageCounter  = 0;
        uint32_t    m_delaySamples  = 0;
        uint32_t    m_holdSamples   = 0;
    };

    // Flex envelope generator (SFZ v2 egN).
    // Arbitrary multi-point envelope with optional sustain/loop points.
    class FlexEnvelope
    {
    public:
        FlexEnvelope() = default;

        // Configure from parsed parameters.
        void configure(const FlexEGParams& params, SampleRate sampleRate);

        void noteOn();
        void noteOff();

        // Process one sample. Returns the envelope value.
        float process();

        float getValue() const { return m_value; }
        bool  isFinished() const { return m_finished; }

    private:
        struct ResolvedPoint
        {
            uint32_t    durationSamples = 0;
            float       targetLevel     = 0.0f;
            float       shape           = 0.0f;    // 0 = linear
        };

        std::vector<ResolvedPoint>  m_points;
        int                         m_sustainPoint = -1;
        int                         m_loopPoint    = -1;
        int                         m_loopCount    = 0;

        int         m_currentPoint  = 0;
        uint32_t    m_sampleCounter = 0;
        int         m_loopsRemaining = 0;
        float       m_value         = 0.0f;
        float       m_startLevel    = 0.0f;
        bool        m_released      = false;
        bool        m_finished      = false;
        SampleRate  m_sampleRate    = 44100;
    };

} // namespace sfz
