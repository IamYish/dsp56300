#pragma once

#include "engine/voice.h"
#include "sfz/instrument.h"
#include "sample/sampleManager.h"
#include "engine/modMatrix.h"
#include "core/config.h"

#include <array>
#include <vector>

namespace sfz
{
    // Manages voice allocation, stealing, group muting, and polyphony limits.
    class VoiceManager
    {
    public:
        VoiceManager();

        // Set the current instrument and sample manager.
        void setInstrument(const Instrument* instrument, const SampleManager* sampleMgr);

        // Set the sample rate for all voices.
        void setSampleRate(SampleRate sampleRate);

        // Handle a note-on event: allocate voices for all matching regions.
        void noteOn(MidiNote note, MidiVel velocity, MidiChannel channel,
                    const ModMatrix& modMatrix);

        // Handle a note-off event: release all voices playing this note.
        void noteOff(MidiNote note, MidiVel releaseVelocity, MidiChannel channel);

        // Process all active voices (stereo output).
        void process(float* outputL, float* outputR, uint32_t numFrames,
                     const ModMatrix& modMatrix);

        // Get the number of currently active voices.
        int getActiveVoiceCount() const;

        // Kill all voices immediately.
        void allNotesOff();

        // Kill all sound immediately (no release).
        void allSoundOff();

    private:
        // Find an idle voice, or steal one if none available.
        Voice* allocateVoice();

        // Apply group muting (off_by) for a region about to play.
        void applyGroupMuting(const Region& region);

        std::array<Voice, Config::MaxVoices>    m_voices;
        const Instrument*                       m_instrument    = nullptr;
        const SampleManager*                    m_sampleManager = nullptr;
        SampleRate                              m_sampleRate    = Config::DefaultSampleRate;

        // Round-robin and random state (per-group tracking)
        struct GroupState
        {
            int     roundRobinCounter = 0;
            bool    alternate = false;
        };
        std::vector<GroupState>                  m_groupStates;

        // Keyswitch state
        MidiNote                                m_lastKeyswitch = 255;
    };

} // namespace sfz
