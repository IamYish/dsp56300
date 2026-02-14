#include "voiceManager.h"

#include <random>
#include <cmath>

namespace sfz
{
    VoiceManager::VoiceManager() = default;

    void VoiceManager::setInstrument(const Instrument* instrument, const SampleManager* sampleMgr)
    {
        m_instrument = instrument;
        m_sampleManager = sampleMgr;

        allSoundOff();
        m_lastKeyswitch = 255;
        m_groupStates.clear();
    }

    void VoiceManager::setSampleRate(SampleRate sampleRate)
    {
        m_sampleRate = sampleRate;
    }

    void VoiceManager::noteOn(MidiNote note, MidiVel velocity, MidiChannel channel,
                              const ModMatrix& modMatrix)
    {
        if (!m_instrument || !m_sampleManager || velocity == 0)
        {
            if (velocity == 0)
                noteOff(note, 64, channel);
            return;
        }

        // Check for keyswitch
        for (const auto& region : m_instrument->regions)
        {
            if (note >= region.keyswitch.swLoKey && note <= region.keyswitch.swHiKey)
            {
                m_lastKeyswitch = note;
                // Keyswitches might not trigger sample playback
            }
        }

        // Find matching regions
        auto matchingRegions = m_instrument->findRegions(note, velocity, channel);

        // Random for probability-based selection
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> randDist(0.0f, 1.0f);
        const float randVal = randDist(rng);

        for (const auto* region : matchingRegions)
        {
            // Check trigger type
            if (region->trigger != TriggerType::Attack &&
                region->trigger != TriggerType::First &&
                region->trigger != TriggerType::Legato)
                continue;

            // Check random condition
            if (randVal < region->loRand || randVal >= region->hiRand)
                continue;

            // Check keyswitch condition
            if (region->keyswitch.swLast != 255)
            {
                if (m_lastKeyswitch != region->keyswitch.swLast)
                    continue;
            }

            // Check round-robin (seq_position / seq_length)
            if (region->seqLength > 1)
            {
                int groupIdx = region->sourceGroupIndex;
                if (groupIdx >= 0)
                {
                    if (groupIdx >= static_cast<int>(m_groupStates.size()))
                        m_groupStates.resize(groupIdx + 1);

                    auto& gs = m_groupStates[groupIdx];
                    gs.roundRobinCounter++;
                    if (gs.roundRobinCounter > region->seqLength)
                        gs.roundRobinCounter = 1;

                    if (gs.roundRobinCounter != region->seqPosition)
                        continue;
                }
            }

            // Get the sample
            const SampleData* sample = m_sampleManager->getSample(region->sampleIndex);
            if (!sample)
                continue;

            // Apply group muting before allocating voice
            applyGroupMuting(*region);

            // Allocate a voice
            Voice* voice = allocateVoice();
            if (!voice)
                break; // all voices exhausted

            voice->noteOn(*region, sample, note, velocity, modMatrix, m_sampleRate);
        }
    }

    void VoiceManager::noteOff(MidiNote note, MidiVel releaseVelocity, MidiChannel /*channel*/)
    {
        for (auto& voice : m_voices)
        {
            if (voice.isPlaying() && voice.getNote() == note &&
                voice.getState() != Voice::State::Stealing)
            {
                voice.noteOff(releaseVelocity);
            }
        }
    }

    void VoiceManager::process(float* outputL, float* outputR, uint32_t numFrames,
                               const ModMatrix& modMatrix)
    {
        for (auto& voice : m_voices)
        {
            if (voice.isPlaying())
                voice.process(outputL, outputR, numFrames, modMatrix);
        }
    }

    int VoiceManager::getActiveVoiceCount() const
    {
        int count = 0;
        for (const auto& voice : m_voices)
        {
            if (voice.isPlaying())
                ++count;
        }
        return count;
    }

    void VoiceManager::allNotesOff()
    {
        for (auto& voice : m_voices)
        {
            if (voice.getState() == Voice::State::Playing)
                voice.noteOff();
        }
    }

    void VoiceManager::allSoundOff()
    {
        for (auto& voice : m_voices)
        {
            if (voice.isPlaying())
                voice.steal(0.001f);
        }
    }

    Voice* VoiceManager::allocateVoice()
    {
        // First: find an idle voice
        for (auto& voice : m_voices)
        {
            if (voice.isIdle())
                return &voice;
        }

        // Second: steal the oldest voice in release phase
        for (auto& voice : m_voices)
        {
            if (voice.getState() == Voice::State::Release)
            {
                voice.steal();
                return &voice;
            }
        }

        // Third: steal the oldest playing voice
        // For now, just grab the first one
        m_voices[0].steal();
        return &m_voices[0];
    }

    void VoiceManager::applyGroupMuting(const Region& region)
    {
        if (region.voiceGroup == 0)
            return;

        for (auto& voice : m_voices)
        {
            if (!voice.isPlaying())
                continue;

            // Check if this voice should be muted by the new region's group
            if (voice.getVoiceGroup() == region.offBy || region.offBy == region.voiceGroup)
            {
                if (region.offMode == OffMode::Fast)
                    voice.steal(0.006f);
                else
                    voice.steal(region.offTime);
            }
        }
    }

} // namespace sfz
