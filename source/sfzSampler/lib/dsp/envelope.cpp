#include "envelope.h"

#include <algorithm>

namespace sfz
{
    // ---- DAHDSR Envelope ----

    void DAHDSREnvelope::configure(const EnvelopeParams& params, float velocity,
                                   SampleRate sampleRate)
    {
        m_sampleRate = static_cast<float>(sampleRate);

        // Velocity modulation (0-1 range for velocity)
        const float velNorm = velocity / 127.0f;

        m_delayTime     = std::max(0.0f, params.delay.base + params.vel2delay * velNorm);
        m_startLevel    = std::clamp(params.start.base / 100.0f, 0.0f, 1.0f);
        m_attackTime    = std::max(0.0f, params.attack.base + params.vel2attack * velNorm);
        m_holdTime      = std::max(0.0f, params.hold.base + params.vel2hold * velNorm);
        m_decayTime     = std::max(0.0f, params.decay.base + params.vel2decay * velNorm);
        m_sustainLevel  = std::clamp((params.sustain.base + params.vel2sustain * velNorm) / 100.0f, 0.0f, 1.0f);
        m_releaseTime   = std::max(0.001f, params.release.base + params.vel2release * velNorm);

        m_delaySamples = static_cast<uint32_t>(m_delayTime * m_sampleRate);
        m_holdSamples  = static_cast<uint32_t>(m_holdTime * m_sampleRate);

        m_attackRate  = computeRate(m_attackTime);
        m_decayRate   = computeRate(m_decayTime);
        m_releaseRate = computeRate(m_releaseTime);
    }

    void DAHDSREnvelope::noteOn()
    {
        m_value = m_startLevel;
        m_stageCounter = 0;

        if (m_delaySamples > 0)
            m_stage = Stage::Delay;
        else if (m_attackTime > 0.0f)
            m_stage = Stage::Attack;
        else
        {
            m_value = 1.0f;
            if (m_holdSamples > 0)
                m_stage = Stage::Hold;
            else if (m_decayTime > 0.0f)
                m_stage = Stage::Decay;
            else
                m_stage = Stage::Sustain;
        }
    }

    void DAHDSREnvelope::noteOff()
    {
        if (m_stage != Stage::Finished && m_stage != Stage::Idle)
            m_stage = Stage::Release;
    }

    float DAHDSREnvelope::process()
    {
        switch (m_stage)
        {
        case Stage::Idle:
        case Stage::Finished:
            return m_value;

        case Stage::Delay:
            if (++m_stageCounter >= m_delaySamples)
                advanceStage();
            return m_value;

        case Stage::Attack:
            m_value += m_attackRate;
            if (m_value >= 1.0f)
            {
                m_value = 1.0f;
                advanceStage();
            }
            return m_value;

        case Stage::Hold:
            if (++m_stageCounter >= m_holdSamples)
                advanceStage();
            return m_value;

        case Stage::Decay:
            m_value -= m_decayRate;
            if (m_value <= m_sustainLevel)
            {
                m_value = m_sustainLevel;
                advanceStage();
            }
            return m_value;

        case Stage::Sustain:
            return m_value;

        case Stage::Release:
            m_value -= m_releaseRate;
            if (m_value <= 0.0f)
            {
                m_value = 0.0f;
                m_stage = Stage::Finished;
            }
            return m_value;
        }

        return m_value;
    }

    void DAHDSREnvelope::advanceStage()
    {
        m_stageCounter = 0;

        switch (m_stage)
        {
        case Stage::Delay:
            m_stage = (m_attackTime > 0.0f) ? Stage::Attack : Stage::Hold;
            break;
        case Stage::Attack:
            m_stage = (m_holdSamples > 0) ? Stage::Hold : Stage::Decay;
            break;
        case Stage::Hold:
            m_stage = (m_decayTime > 0.0f) ? Stage::Decay : Stage::Sustain;
            break;
        case Stage::Decay:
            m_stage = Stage::Sustain;
            break;
        default:
            break;
        }
    }

    float DAHDSREnvelope::computeRate(float timeSeconds) const
    {
        if (timeSeconds <= 0.0f)
            return 1.0f; // instant
        return 1.0f / (timeSeconds * m_sampleRate);
    }

    // ---- Flex Envelope ----

    void FlexEnvelope::configure(const FlexEGParams& params, SampleRate sampleRate)
    {
        m_sampleRate = sampleRate;
        m_sustainPoint = params.sustainPoint;
        m_loopPoint = params.loopPoint;
        m_loopCount = params.loopCount;

        m_points.clear();
        m_points.reserve(params.points.size());

        for (const auto& pt : params.points)
        {
            ResolvedPoint rp;
            rp.durationSamples = static_cast<uint32_t>(
                std::max(0.0f, pt.time.base) * static_cast<float>(sampleRate));
            rp.targetLevel = pt.level.base;
            rp.shape = pt.shape;
            m_points.push_back(rp);
        }
    }

    void FlexEnvelope::noteOn()
    {
        m_currentPoint = 0;
        m_sampleCounter = 0;
        m_loopsRemaining = m_loopCount;
        m_released = false;
        m_finished = false;
        m_value = m_points.empty() ? 0.0f : m_points[0].targetLevel;
        m_startLevel = m_value;
    }

    void FlexEnvelope::noteOff()
    {
        m_released = true;
        // If we're at the sustain point, advance past it
        if (m_sustainPoint >= 0 && m_currentPoint == m_sustainPoint)
        {
            m_currentPoint++;
            m_sampleCounter = 0;
            m_startLevel = m_value;
        }
    }

    float FlexEnvelope::process()
    {
        if (m_finished || m_points.empty())
            return m_value;

        if (m_currentPoint >= static_cast<int>(m_points.size()))
        {
            m_finished = true;
            return m_value;
        }

        // Check sustain hold
        if (!m_released && m_sustainPoint >= 0 && m_currentPoint == m_sustainPoint)
        {
            // Hold at sustain point
            return m_value;
        }

        const auto& point = m_points[m_currentPoint];

        if (point.durationSamples > 0)
        {
            const float progress = static_cast<float>(m_sampleCounter) /
                                   static_cast<float>(point.durationSamples);

            // Apply shape (0 = linear, >0 = exponential, <0 = logarithmic)
            float shaped = progress;
            if (point.shape > 0.0f)
                shaped = std::pow(progress, point.shape + 1.0f);
            else if (point.shape < 0.0f)
                shaped = 1.0f - std::pow(1.0f - progress, 1.0f - point.shape);

            m_value = m_startLevel + shaped * (point.targetLevel - m_startLevel);
        }
        else
        {
            m_value = point.targetLevel;
        }

        ++m_sampleCounter;

        if (m_sampleCounter >= point.durationSamples)
        {
            m_value = point.targetLevel;
            m_startLevel = m_value;
            ++m_currentPoint;
            m_sampleCounter = 0;

            // Handle looping
            if (m_loopPoint >= 0 && m_currentPoint > m_sustainPoint && !m_released)
            {
                if (m_loopCount == 0 || m_loopsRemaining > 0)
                {
                    m_currentPoint = m_loopPoint;
                    if (m_loopsRemaining > 0)
                        --m_loopsRemaining;
                }
            }
        }

        return m_value;
    }

} // namespace sfz
