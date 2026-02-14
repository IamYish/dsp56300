#pragma once

#include "core/types.h"
#include "core/config.h"

#include <functional>
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace sfz
{
    // Callback for incoming MIDI messages.
    using MidiInputCallback = std::function<void(const MidiMessage& msg)>;

    // WinRT MIDI input backend.
    //
    // Uses the Windows Runtime MIDI API (Windows.Devices.Midi) for MIDI input.
    // WinRT MIDI provides lower latency and better device enumeration than
    // the legacy WinMM midiIn API.
    //
    // Default configuration:
    //   - Port 3 (zero-indexed, configurable)
    //
    // The MIDI callback is invoked from a WinRT thread pool thread.
    // Messages should be forwarded to the engine via the lock-free ring buffer.
    class WinrtMidi
    {
    public:
        WinrtMidi();
        ~WinrtMidi();

        // Non-copyable
        WinrtMidi(const WinrtMidi&) = delete;
        WinrtMidi& operator=(const WinrtMidi&) = delete;

        // List available MIDI input ports.
        static std::vector<std::string> getInputPortList();

        // Open a MIDI input port.
        // portIndex: 0-based index into the port list (default: Config::DefaultMidiPort = 3).
        // Returns true on success.
        bool open(MidiInputCallback callback, uint32_t portIndex = Config::DefaultMidiPort);

        // Close the MIDI input port.
        void close();

        // Query state
        bool isOpen() const { return m_isOpen; }
        uint32_t getPortIndex() const { return m_portIndex; }
        std::string getPortName() const { return m_portName; }

    private:
        // Parse raw MIDI bytes into a MidiMessage.
        static MidiMessage parseMidiBytes(const uint8_t* data, uint32_t length, uint32_t timestamp);

        MidiInputCallback   m_callback;
        uint32_t            m_portIndex     = Config::DefaultMidiPort;
        std::string         m_portName;
        std::atomic<bool>   m_isOpen{false};

        // Platform-specific opaque handle (WinRT COM pointers).
        // Using void* to avoid pulling WinRT headers into the header file.
        void*               m_midiInPort    = nullptr;
        void*               m_deviceWatcher = nullptr;
    };

} // namespace sfz
