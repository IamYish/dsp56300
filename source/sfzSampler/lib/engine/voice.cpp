#include "voice.h"

#include <cmath>
#include <algorithm>
#include <random>

namespace sfz
{
    void Voice::noteOn(const Region& region, const SampleData* sample,
                       MidiNote note, MidiVel velocity, const ModMatrix& modMatrix,
                       SampleRate sampleRate)
    {
        m_state = State::Playing;
        m_region = &region;
        m_sample = sample;
        m_note = note;
        m_velocity = velocity;
        m_sampleRate = sampleRate;
        m_voiceGroup = region.voiceGroup;
        m_regionGroupIndex = region.sourceGroupIndex;
        m_stealFade = 1.0f;
        m_stealFadeRate = 0.0f;

        // Cache region parameters
        m_regionVolume = modMatrix.resolve(region.volume);
        m_regionAmplitude = modMatrix.resolve(region.amplitude);
        m_pitchKeycenter = static_cast<float>(region.pitchKeycenter);
        m_pitchKeytrack = region.pitchKeytrack;
        m_transpose = region.transpose;
        m_tune = modMatrix.resolve(region.tune);
        m_bendRange = static_cast<float>(region.bendUp);

        // Compute gain
        // Volume is in dB, amplitude is 0-100%
        const float volumeDb = m_regionVolume;
        const float ampFactor = m_regionAmplitude / 100.0f;
        const float velFactor = velocity / 127.0f; // simplified; real impl uses amp_veltrack + curves

        // Apply amp_veltrack
        const float velTracked = 1.0f - region.ampVeltrack / 100.0f * (1.0f - velFactor);

        m_gainLinear = std::pow(10.0f, volumeDb / 20.0f) * ampFactor * velTracked;

        // Apply random gain variation
        if (region.ampRandom > 0.0f)
        {
            static thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(-region.ampRandom, region.ampRandom);
            m_gainLinear *= std::pow(10.0f, dist(rng) / 20.0f);
        }

        // Pan (-100 to 100 -> L/R gains using constant-power law)
        const float panNorm = modMatrix.resolve(region.pan) / 100.0f; // -1 to 1
        const float panAngle = (panNorm + 1.0f) * 0.25f * 3.14159265f; // 0 to pi/2
        m_panL = std::cos(panAngle);
        m_panR = std::sin(panAngle);

        // Phase inversion
        if (region.phase)
            m_gainLinear = -m_gainLinear;

        // Sample playback position
        const int offset = modMatrix.resolveInt(region.offset);
        m_playbackPos = static_cast<double>(std::max(0, offset));

        // Direction
        m_forward = region.direction;

        // Loop
        m_looping = (region.loopMode == LoopMode::LoopContinuous ||
                     region.loopMode == LoopMode::LoopSustain);
        if (sample)
        {
            m_loopStart = (region.loopStart >= 0) ? region.loopStart :
                          static_cast<int>(sample->getLoopStart());
            m_loopEnd = (region.loopEnd >= 0) ? region.loopEnd :
                        static_cast<int>(sample->getLoopEnd());
            if (m_loopEnd == 0 && m_looping)
                m_loopEnd = static_cast<int>(sample->getFrameCount()) - 1;
        }
        m_loopType = region.loopType;

        // Compute playback rate
        m_playbackRate = computePlaybackRate(modMatrix);

        // Configure envelopes
        m_ampEG.configure(region.ampEG, static_cast<float>(velocity), sampleRate);
        m_ampEG.noteOn();

        if (region.filterEG.depth != 0.0f || region.filterEG.attack.base > 0.0f)
        {
            m_filterEG.configure(region.filterEG, static_cast<float>(velocity), sampleRate);
            m_filterEG.noteOn();
        }

        if (region.pitchEG.depth != 0.0f || region.pitchEG.attack.base > 0.0f)
        {
            m_pitchEG.configure(region.pitchEG, static_cast<float>(velocity), sampleRate);
            m_pitchEG.noteOn();
        }

        // Configure flex envelopes
        m_flexEGs.resize(region.flexEGs.size());
        for (size_t i = 0; i < region.flexEGs.size(); ++i)
        {
            m_flexEGs[i].configure(region.flexEGs[i], sampleRate);
            m_flexEGs[i].noteOn();
        }

        // Configure LFOs
        if (region.ampLFO.freq.base > 0.0f)
        {
            m_ampLFO.configure(region.ampLFO, sampleRate);
            m_ampLFO.noteOn();
        }
        if (region.filterLFO.freq.base > 0.0f)
        {
            m_filterLFO.configure(region.filterLFO, sampleRate);
            m_filterLFO.noteOn();
        }
        if (region.pitchLFO.freq.base > 0.0f)
        {
            m_pitchLFO.configure(region.pitchLFO, sampleRate);
            m_pitchLFO.noteOn();
        }

        m_flexLFOs.resize(region.flexLFOs.size());
        for (size_t i = 0; i < region.flexLFOs.size(); ++i)
        {
            m_flexLFOs[i].configure(region.flexLFOs[i], sampleRate);
            m_flexLFOs[i].noteOn();
        }

        // Configure filters
        for (int f = 0; f < Config::MaxFilters; ++f)
        {
            const auto& fp = region.filters[f];
            if (fp.type != FilterType::None && fp.cutoff.base > 0.0f)
            {
                float cutoff = modMatrix.resolve(fp.cutoff);
                // Apply keytrack
                cutoff += fp.keytrack * (static_cast<float>(note) - static_cast<float>(fp.keycenter));
                // Apply veltrack
                cutoff += fp.veltrack * (static_cast<float>(velocity) / 127.0f);
                cutoff = std::clamp(cutoff, 20.0f, 20000.0f);

                m_filters[f].configure(fp.type, cutoff, modMatrix.resolve(fp.resonance), sampleRate);
            }
            else
            {
                m_filters[f].configure(FilterType::None, 20000.0f, 0.0f, sampleRate);
            }
        }

        // Configure EQ
        m_eq.configure(region.eq, static_cast<float>(velocity), sampleRate);
    }

    void Voice::noteOff(MidiVel /*releaseVelocity*/)
    {
        if (m_state == State::Playing)
        {
            m_state = State::Release;
            m_ampEG.noteOff();
            m_filterEG.noteOff();
            m_pitchEG.noteOff();

            for (auto& eg : m_flexEGs)
                eg.noteOff();

            // For loop_sustain mode, stop looping on note off
            if (m_region && m_region->loopMode == LoopMode::LoopSustain)
                m_looping = false;
        }
    }

    void Voice::steal(float fadeTimeSec)
    {
        m_state = State::Stealing;
        m_stealFadeRate = 1.0f / (fadeTimeSec * static_cast<float>(m_sampleRate));
    }

    void Voice::process(float* outputL, float* outputR, uint32_t numFrames,
                        const ModMatrix& modMatrix)
    {
        if (m_state == State::Idle || !m_sample || m_sample->empty())
            return;

        const uint32_t sampleFrames = m_sample->getFrameCount();
        const uint32_t channels = m_sample->getChannels();
        const bool isOneShot = m_region && (m_region->loopMode == LoopMode::OneShot);

        for (uint32_t i = 0; i < numFrames; ++i)
        {
            // Process amp envelope
            const float ampEnv = m_ampEG.process();

            // Check if we're done
            if (m_ampEG.isFinished() || m_playbackPos >= static_cast<double>(sampleFrames))
            {
                if (!isOneShot || m_playbackPos >= static_cast<double>(sampleFrames))
                {
                    m_state = State::Idle;
                    return;
                }
            }

            // Process pitch LFO and envelope for real-time pitch modulation
            float pitchMod = 0.0f;
            if (m_region && m_region->pitchLFO.freq.base > 0.0f)
                pitchMod += m_pitchLFO.process();
            if (m_region && m_region->pitchEG.depth != 0.0f)
                pitchMod += m_pitchEG.process() * m_region->pitchEG.depth;

            // Compute per-sample playback rate with pitch modulation
            double rate = m_playbackRate;
            if (pitchMod != 0.0f)
                rate *= std::pow(2.0, static_cast<double>(pitchMod) / 1200.0);

            // Read sample with interpolation
            float sampleL, sampleR;
            if (channels >= 2)
            {
                sampleL = m_sample->getSampleInterpolated(m_playbackPos, 0);
                sampleR = m_sample->getSampleInterpolated(m_playbackPos, 1);
            }
            else
            {
                sampleL = sampleR = m_sample->getSampleInterpolated(m_playbackPos, 0);
            }

            // Advance playback position
            if (m_forward)
                m_playbackPos += rate;
            else
                m_playbackPos -= rate;

            // Handle looping
            if (m_looping && m_loopEnd > m_loopStart)
            {
                switch (m_loopType)
                {
                case LoopType::Forward:
                    if (m_playbackPos >= static_cast<double>(m_loopEnd))
                        m_playbackPos -= static_cast<double>(m_loopEnd - m_loopStart);
                    break;
                case LoopType::Backward:
                    if (m_playbackPos < static_cast<double>(m_loopStart))
                        m_playbackPos += static_cast<double>(m_loopEnd - m_loopStart);
                    break;
                case LoopType::Alternate:
                    if (m_forward && m_playbackPos >= static_cast<double>(m_loopEnd))
                    {
                        m_playbackPos = static_cast<double>(m_loopEnd) * 2.0 - m_playbackPos;
                        m_forward = false;
                    }
                    else if (!m_forward && m_playbackPos < static_cast<double>(m_loopStart))
                    {
                        m_playbackPos = static_cast<double>(m_loopStart) * 2.0 - m_playbackPos;
                        m_forward = true;
                    }
                    break;
                }
            }

            // Apply filters
            for (auto& filter : m_filters)
            {
                sampleL = filter.process(sampleL);
                sampleR = filter.process(sampleR);
            }

            // Apply EQ
            if (m_eq.isActive())
            {
                sampleL = m_eq.process(sampleL);
                sampleR = m_eq.process(sampleR);
            }

            // Process amp LFO
            float ampLfoGain = 1.0f;
            if (m_region && m_region->ampLFO.freq.base > 0.0f)
            {
                const float lfoVal = m_ampLFO.process();
                ampLfoGain = std::pow(10.0f, lfoVal / 20.0f);
            }

            // Final gain
            float gain = m_gainLinear * ampEnv * ampLfoGain;

            // Handle voice stealing fade
            if (m_state == State::Stealing)
            {
                m_stealFade -= m_stealFadeRate;
                if (m_stealFade <= 0.0f)
                {
                    m_state = State::Idle;
                    return;
                }
                gain *= m_stealFade;
            }

            // Apply pan and write to output (additive)
            outputL[i] += sampleL * gain * m_panL;
            outputR[i] += sampleR * gain * m_panR;
        }
    }

    double Voice::computePlaybackRate(const ModMatrix& modMatrix) const
    {
        if (!m_sample || !m_region)
            return 1.0;

        // Base rate from sample rate mismatch
        double rate = static_cast<double>(m_sample->getSampleRate()) /
                      static_cast<double>(m_sampleRate);

        // Pitch from note vs pitch_keycenter
        double pitchCents = static_cast<double>(m_note - m_region->pitchKeycenter) *
                            static_cast<double>(m_pitchKeytrack);

        // Add transpose and fine tune
        pitchCents += static_cast<double>(m_transpose) * 100.0;
        pitchCents += static_cast<double>(m_tune);

        // Add pitch bend
        const float bendNorm = modMatrix.getCC(CC_PitchBend); // 0 to 1
        const float bendBipolar = (bendNorm - 0.5f) * 2.0f; // -1 to 1
        pitchCents += static_cast<double>(bendBipolar) * static_cast<double>(m_bendRange);

        // Convert cents to rate multiplier
        rate *= std::pow(2.0, pitchCents / 1200.0);

        return rate;
    }

} // namespace sfz
