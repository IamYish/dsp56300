#include "engine.h"

#include <filesystem>
#include <cstring>

namespace sfz
{
    Engine::Engine() = default;
    Engine::~Engine() = default;

    void Engine::setSampleRate(SampleRate sampleRate)
    {
        m_sampleRate = sampleRate;
        m_voiceManager.setSampleRate(sampleRate);
    }

    void Engine::setBufferSize(uint32_t bufferSize)
    {
        m_bufferSize = bufferSize;
    }

    bool Engine::loadInstrument(const std::string& sfzFilePath)
    {
        std::lock_guard<std::mutex> lock(m_loadMutex);

        m_parserMessages.clear();

        // Parse the SFZ file
        Parser parser;
        parser.setCallback([this](const std::string& msg, int line, bool /*isError*/) {
            m_parserMessages.push_back("[Line " + std::to_string(line) + "] " + msg);
        });

        auto newInstrument = parser.parse(sfzFilePath);
        if (!newInstrument)
        {
            m_parserMessages.push_back("Failed to parse: " + sfzFilePath);
            return false;
        }

        // Load all referenced samples
        const auto basePath = std::filesystem::path(sfzFilePath).parent_path();
        m_sampleManager.clear();
        const int loaded = m_sampleManager.loadInstrumentSamples(*newInstrument, basePath);

        m_parserMessages.push_back(
            "Loaded " + std::to_string(loaded) + " samples, " +
            std::to_string(newInstrument->regions.size()) + " regions");

        // Set up effect buses
        m_effectBuses.clear();
        for (const auto& effectParams : newInstrument->effects)
        {
            auto effect = createEffect(effectParams, m_sampleRate);
            if (effect)
            {
                if (effectParams.bus >= static_cast<int>(m_effectBuses.size()))
                    m_effectBuses.resize(effectParams.bus + 1);
                m_effectBuses[effectParams.bus].addEffect(std::move(effect));
            }
        }

        // Swap in the new instrument
        m_instrument = std::move(newInstrument);

        // Configure voice manager
        m_voiceManager.setInstrument(m_instrument.get(), &m_sampleManager);
        m_voiceManager.setSampleRate(m_sampleRate);

        // Configure modulation matrix
        m_modMatrix.setInstrument(m_instrument.get());
        m_modMatrix.initDefaults();

        return true;
    }

    void Engine::unloadInstrument()
    {
        std::lock_guard<std::mutex> lock(m_loadMutex);

        m_voiceManager.allSoundOff();
        m_voiceManager.setInstrument(nullptr, nullptr);
        m_instrument.reset();
        m_sampleManager.clear();
        m_effectBuses.clear();
        m_modMatrix.setInstrument(nullptr);
    }

    void Engine::processAudio(float* outputL, float* outputR, uint32_t numFrames)
    {
        // Clear output
        std::memset(outputL, 0, numFrames * sizeof(float));
        std::memset(outputR, 0, numFrames * sizeof(float));

        if (!m_instrument)
            return;

        // Process MIDI events for this buffer
        processMidiEvents(numFrames);

        // Process all active voices (additive into output)
        m_voiceManager.process(outputL, outputR, numFrames, m_modMatrix);

        // Process effect buses
        for (auto& bus : m_effectBuses)
            bus.process(outputL, outputR, numFrames);

        // Clamp output to prevent clipping
        for (uint32_t i = 0; i < numFrames; ++i)
        {
            outputL[i] = std::max(-1.0f, std::min(1.0f, outputL[i]));
            outputR[i] = std::max(-1.0f, std::min(1.0f, outputR[i]));
        }
    }

    void Engine::pushMidi(const MidiMessage& msg)
    {
        m_midiQueue.push(msg);
    }

    int Engine::getActiveVoiceCount() const
    {
        return m_voiceManager.getActiveVoiceCount();
    }

    size_t Engine::getSampleMemoryUsage() const
    {
        return m_sampleManager.getMemoryUsage();
    }

    void Engine::processMidiEvents(uint32_t /*numFrames*/)
    {
        MidiMessage msg;
        while (m_midiQueue.pop(msg))
        {
            handleMidi(msg);
        }
    }

    void Engine::handleMidi(const MidiMessage& msg)
    {
        switch (msg.type)
        {
        case MidiMessage::Type::NoteOn:
            if (msg.data2 > 0)
                m_voiceManager.noteOn(msg.data1, msg.data2, msg.channel, m_modMatrix);
            else
                m_voiceManager.noteOff(msg.data1, 64, msg.channel);
            break;

        case MidiMessage::Type::NoteOff:
            m_voiceManager.noteOff(msg.data1, msg.data2, msg.channel);
            break;

        case MidiMessage::Type::ControlChange:
        {
            const MidiCC cc = msg.data1;
            const float value = static_cast<float>(msg.data2) / 127.0f;
            m_modMatrix.setCC(cc, value);

            // Handle special CCs
            if (cc == 120) // All Sound Off
                m_voiceManager.allSoundOff();
            else if (cc == 123) // All Notes Off
                m_voiceManager.allNotesOff();
            break;
        }

        case MidiMessage::Type::PitchBend:
            m_modMatrix.setPitchBend(msg.pitchBend);
            break;

        case MidiMessage::Type::ChannelPressure:
            m_modMatrix.setChannelAT(msg.data1);
            break;

        case MidiMessage::Type::PolyPressure:
            m_modMatrix.setPolyAT(msg.data2);
            break;

        case MidiMessage::Type::ProgramChange:
            // Could be used for instrument switching in the future
            break;
        }
    }

} // namespace sfz
