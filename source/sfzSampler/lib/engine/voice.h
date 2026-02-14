#pragma once

#include "sfz/region.h"
#include "sample/sampleData.h"
#include "dsp/envelope.h"
#include "dsp/lfo.h"
#include "dsp/filter.h"
#include "dsp/eq.h"
#include "engine/modMatrix.h"
#include "core/config.h"

#include <array>
#include <vector>

namespace sfz
{
    // A single sampler voice.
    // Each voice plays one region with full DSP processing:
    //   - Sample playback with pitch shifting
    //   - 2 serial filters
    //   - 3-band parametric EQ
    //   - DAHDSR envelopes (amp, filter, pitch)
    //   - Flex envelopes (egN)
    //   - Fixed LFOs (amp, filter, pitch)
    //   - Flex LFOs (lfoN)
    //   - Amplitude, pan, width
    class Voice
    {
    public:
        enum class State
        {
            Idle,       // Voice is not playing
            Playing,    // Voice is actively playing
            Release,    // Voice is in release phase
            Stealing,   // Voice is being stolen (fast fade out)
        };

        Voice() = default;

        // Start playing a note with the given region.
        void noteOn(const Region& region, const SampleData* sample,
                    MidiNote note, MidiVel velocity, const ModMatrix& modMatrix,
                    SampleRate sampleRate);

        // Release the note.
        void noteOff(MidiVel releaseVelocity = 64);

        // Force-stop with fast fade (voice stealing).
        void steal(float fadeTimeSec = 0.006f);

        // Process audio for this voice (stereo interleaved output).
        // Adds to the output buffer (does not overwrite).
        void process(float* outputL, float* outputR, uint32_t numFrames,
                     const ModMatrix& modMatrix);

        // State queries
        State       getState() const        { return m_state; }
        bool        isIdle() const          { return m_state == State::Idle; }
        bool        isPlaying() const       { return m_state != State::Idle; }
        MidiNote    getNote() const         { return m_note; }
        MidiVel     getVelocity() const     { return m_velocity; }
        int         getVoiceGroup() const   { return m_voiceGroup; }
        int         getRegionGroupIndex() const { return m_regionGroupIndex; }

    private:
        // Compute the playback speed ratio for current pitch settings.
        double computePlaybackRate(const ModMatrix& modMatrix) const;

        State               m_state             = State::Idle;
        const Region*       m_region            = nullptr;
        const SampleData*   m_sample            = nullptr;

        MidiNote            m_note              = 60;
        MidiVel             m_velocity          = 100;
        SampleRate          m_sampleRate        = 44100;
        int                 m_voiceGroup        = 0;
        int                 m_regionGroupIndex  = -1;

        // Playback position (double for sub-sample precision)
        double              m_playbackPos       = 0.0;
        double              m_playbackRate      = 1.0;
        bool                m_looping           = false;
        int                 m_loopStart         = 0;
        int                 m_loopEnd           = 0;
        LoopType            m_loopType          = LoopType::Forward;
        bool                m_forward           = true; // current direction for alternate loop

        // Amplitude
        float               m_gainLinear        = 1.0f;
        float               m_panL              = 1.0f;
        float               m_panR              = 1.0f;

        // Stealing fade
        float               m_stealFade         = 1.0f;
        float               m_stealFadeRate     = 0.0f;

        // DSP: envelopes
        DAHDSREnvelope      m_ampEG;
        DAHDSREnvelope      m_filterEG;
        DAHDSREnvelope      m_pitchEG;
        std::vector<FlexEnvelope>   m_flexEGs;

        // DSP: LFOs
        FixedLFO            m_ampLFO;
        FixedLFO            m_filterLFO;
        FixedLFO            m_pitchLFO;
        std::vector<FlexLFO>        m_flexLFOs;

        // DSP: filters
        std::array<BiquadFilter, Config::MaxFilters>    m_filters;

        // DSP: EQ
        ParametricEQ        m_eq;

        // Cached region parameters
        float               m_regionVolume      = 0.0f;    // dB
        float               m_regionAmplitude   = 100.0f;
        float               m_pitchKeycenter    = 60.0f;
        float               m_pitchKeytrack     = 100.0f;
        int                 m_transpose         = 0;
        float               m_tune              = 0.0f;
        float               m_bendRange         = 200.0f;
    };

} // namespace sfz
