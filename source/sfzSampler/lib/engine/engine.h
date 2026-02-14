#pragma once

#include "engine/voiceManager.h"
#include "engine/modMatrix.h"
#include "sfz/instrument.h"
#include "sfz/parser.h"
#include "sample/sampleManager.h"
#include "dsp/effects.h"
#include "core/types.h"
#include "core/config.h"
#include "core/ringBuffer.h"

#include <string>
#include <memory>
#include <vector>
#include <mutex>

namespace sfz
{
    // Main sampler engine.
    //
    // This is the top-level class that both the standalone application and a
    // future VST3 plugin link against. It owns:
    //   - SFZ parser
    //   - Sample manager
    //   - Voice manager
    //   - Modulation matrix
    //   - Effect buses
    //
    // The engine integrates with the dsp56300 emulator: audio output from the
    // voice manager is routed through the DSP56300's audio subsystem, and the
    // DSP can optionally provide additional processing (effects, filtering).
    //
    // Thread model:
    //   - processAudio() is called from the audio thread (real-time safe)
    //   - loadInstrument() is called from the main/GUI thread
    //   - MIDI events are pushed into a lock-free ring buffer from any thread
    class Engine
    {
    public:
        Engine();
        ~Engine();

        // Non-copyable
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        // ---- Configuration ----

        // Set audio parameters. Must be called before processAudio().
        void setSampleRate(SampleRate sampleRate);
        void setBufferSize(uint32_t bufferSize);

        SampleRate  getSampleRate() const   { return m_sampleRate; }
        uint32_t    getBufferSize() const   { return m_bufferSize; }

        // ---- Instrument loading (main thread) ----

        // Load an SFZ instrument from a file path.
        // Returns true on success.
        bool loadInstrument(const std::string& sfzFilePath);

        // Unload the current instrument and free all resources.
        void unloadInstrument();

        // Check if an instrument is loaded.
        bool isInstrumentLoaded() const { return m_instrument != nullptr; }

        // Get the loaded instrument (for GUI display).
        const Instrument* getInstrument() const { return m_instrument.get(); }

        // ---- Audio processing (audio thread) ----

        // Process one buffer of audio.
        // Output is interleaved stereo float [-1, 1].
        // This is real-time safe (no allocations, no locks).
        void processAudio(float* outputL, float* outputR, uint32_t numFrames);

        // ---- MIDI input (any thread) ----

        // Push a MIDI message into the engine.
        // Thread-safe (lock-free ring buffer).
        void pushMidi(const MidiMessage& msg);

        // ---- State queries ----
        int  getActiveVoiceCount() const;
        size_t getSampleMemoryUsage() const;

        // Get last parser messages (for GUI display).
        const std::vector<std::string>& getParserMessages() const { return m_parserMessages; }

    private:
        // Process pending MIDI messages for the current audio block.
        void processMidiEvents(uint32_t numFrames);

        // Handle a single MIDI message.
        void handleMidi(const MidiMessage& msg);

        SampleRate      m_sampleRate    = Config::DefaultSampleRate;
        uint32_t        m_bufferSize    = Config::DefaultBufferSize;

        // Core components
        std::unique_ptr<Instrument>     m_instrument;
        SampleManager                   m_sampleManager;
        VoiceManager                    m_voiceManager;
        ModMatrix                       m_modMatrix;

        // Effect buses
        std::vector<EffectBus>          m_effectBuses;

        // MIDI ring buffer (from input thread -> audio thread)
        RingBuffer<MidiMessage, 4096>   m_midiQueue;

        // Parser diagnostic messages
        std::vector<std::string>        m_parserMessages;

        // Loading mutex (protects instrument swap)
        std::mutex                      m_loadMutex;
    };

} // namespace sfz
