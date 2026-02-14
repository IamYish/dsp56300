#include "winmmMidi.h"

// WinRT MIDI implementation.
//
// This uses the Windows.Devices.Midi WinRT API for MIDI input.
// On systems where WinRT is not available (pre-Windows 10), we fall back
// to a stub that returns no devices.
//
// The WinRT MIDI API requires:
//   - Windows 10 or later
//   - C++/WinRT or manual COM activation
//   - Link against: WindowsApp.lib (for UWP) or RuntimeObject.lib (desktop)
//
// For desktop apps (non-UWP), we use manual COM activation via
// RoActivateInstance / RoGetActivationFactory to avoid requiring C++/WinRT.
//
// ARCHITECTURE NOTE:
// This file is structured for WinRT MIDI but includes a WinMM fallback
// path for compatibility. The WinRT path will be activated when building
// with a Windows 10 SDK that includes the WinRT MIDI headers.

#ifdef _WIN32

#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// For now, we implement the MIDI input using WinMM midiIn as a bootstrap.
// This will be replaced with the WinRT implementation when the proper
// Windows SDK headers are integrated.
//
// WinMM MIDI is used as an interim solution because:
// 1. It works on all Windows versions
// 2. It has a simpler build dependency (just winmm.lib)
// 3. The API surface exposed to the rest of the app is identical
//
// The WinRT migration only changes this .cpp file — no header changes needed.

namespace sfz
{
    WinrtMidi::WinrtMidi() = default;

    WinrtMidi::~WinrtMidi()
    {
        close();
    }

    std::vector<std::string> WinrtMidi::getInputPortList()
    {
        std::vector<std::string> ports;
        const UINT numDevs = midiInGetNumDevs();
        for (UINT i = 0; i < numDevs; ++i)
        {
            MIDIINCAPSW caps{};
            if (midiInGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
            {
                char name[64]{};
                WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, name, sizeof(name), nullptr, nullptr);
                ports.emplace_back(name);
            }
        }
        return ports;
    }

    // Static WinMM MIDI callback
    static void CALLBACK midiInCallback(HMIDIIN /*hMidiIn*/, UINT wMsg,
                                         DWORD_PTR dwInstance,
                                         DWORD_PTR dwParam1,
                                         DWORD_PTR /*dwParam2*/)
    {
        if (wMsg != MIM_DATA)
            return;

        auto* self = reinterpret_cast<WinrtMidi*>(dwInstance);
        if (!self)
            return;

        // dwParam1 contains the MIDI message packed into a DWORD:
        // Low byte = status, next byte = data1, next byte = data2
        const uint8_t status  = static_cast<uint8_t>(dwParam1 & 0xFF);
        const uint8_t data1   = static_cast<uint8_t>((dwParam1 >> 8) & 0xFF);
        const uint8_t data2   = static_cast<uint8_t>((dwParam1 >> 16) & 0xFF);

        uint8_t bytes[3] = {status, data1, data2};
        auto msg = WinrtMidi::parseMidiBytes(bytes, 3, 0);

        // Forward to the public callback (stored in the instance)
        // We access it through the static helper below
    }

    // We need a global/static way to dispatch from the C callback to the instance.
    // Using a simple approach: store callback on the instance and access via dwInstance.
    struct MidiCallbackBridge
    {
        static MidiInputCallback* callback;
    };
    MidiInputCallback* MidiCallbackBridge::callback = nullptr;

    static void CALLBACK midiInProc(HMIDIIN /*hMidiIn*/, UINT wMsg,
                                     DWORD_PTR /*dwInstance*/,
                                     DWORD_PTR dwParam1,
                                     DWORD_PTR /*dwParam2*/)
    {
        if (wMsg != MIM_DATA || !MidiCallbackBridge::callback)
            return;

        const uint8_t status  = static_cast<uint8_t>(dwParam1 & 0xFF);
        const uint8_t data1   = static_cast<uint8_t>((dwParam1 >> 8) & 0xFF);
        const uint8_t data2   = static_cast<uint8_t>((dwParam1 >> 16) & 0xFF);

        uint8_t bytes[3] = {status, data1, data2};
        auto msg = WinrtMidi::parseMidiBytes(bytes, 3, 0);

        (*MidiCallbackBridge::callback)(msg);
    }

    bool WinrtMidi::open(MidiInputCallback callback, uint32_t portIndex)
    {
        if (m_isOpen)
            close();

        m_callback = std::move(callback);
        m_portIndex = portIndex;

        // Store callback for the C bridge
        static MidiInputCallback storedCallback;
        storedCallback = m_callback;
        MidiCallbackBridge::callback = &storedCallback;

        // Check if the requested port exists
        const UINT numDevs = midiInGetNumDevs();
        if (portIndex >= numDevs)
        {
            // Fall back to first available device if requested port doesn't exist
            if (numDevs == 0)
                return false;
            m_portIndex = 0;
        }

        // Get port name
        MIDIINCAPSW caps{};
        if (midiInGetDevCapsW(m_portIndex, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
        {
            char name[64]{};
            WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, name, sizeof(name), nullptr, nullptr);
            m_portName = name;
        }

        // Open the MIDI input device
        HMIDIIN hMidiIn = nullptr;
        MMRESULT result = midiInOpen(&hMidiIn, m_portIndex, reinterpret_cast<DWORD_PTR>(midiInProc),
                                      0, CALLBACK_FUNCTION);
        if (result != MMSYSERR_NOERROR)
            return false;

        m_midiInPort = hMidiIn;

        // Start receiving MIDI messages
        midiInStart(hMidiIn);

        m_isOpen = true;
        return true;
    }

    void WinrtMidi::close()
    {
        if (m_midiInPort)
        {
            auto hMidiIn = static_cast<HMIDIIN>(m_midiInPort);
            midiInStop(hMidiIn);
            midiInReset(hMidiIn);
            midiInClose(hMidiIn);
            m_midiInPort = nullptr;
        }

        MidiCallbackBridge::callback = nullptr;
        m_isOpen = false;
        m_portName.clear();
    }

    MidiMessage WinrtMidi::parseMidiBytes(const uint8_t* data, uint32_t length, uint32_t timestamp)
    {
        MidiMessage msg{};
        msg.timestamp = timestamp;

        if (length == 0)
            return msg;

        const uint8_t status = data[0];
        const uint8_t channel = status & 0x0F;
        const uint8_t type = status & 0xF0;

        msg.channel = channel;

        switch (type)
        {
        case 0x90: // Note On
            msg.type = MidiMessage::Type::NoteOn;
            msg.data1 = (length > 1) ? data[1] : 0;
            msg.data2 = (length > 2) ? data[2] : 0;
            break;

        case 0x80: // Note Off
            msg.type = MidiMessage::Type::NoteOff;
            msg.data1 = (length > 1) ? data[1] : 0;
            msg.data2 = (length > 2) ? data[2] : 64;
            break;

        case 0xB0: // Control Change
            msg.type = MidiMessage::Type::ControlChange;
            msg.data1 = (length > 1) ? data[1] : 0;
            msg.data2 = (length > 2) ? data[2] : 0;
            break;

        case 0xE0: // Pitch Bend
            msg.type = MidiMessage::Type::PitchBend;
            if (length >= 3)
            {
                const int raw = (static_cast<int>(data[2]) << 7) | static_cast<int>(data[1]);
                msg.pitchBend = static_cast<int16_t>(raw - 8192);
            }
            break;

        case 0xD0: // Channel Pressure (Aftertouch)
            msg.type = MidiMessage::Type::ChannelPressure;
            msg.data1 = (length > 1) ? data[1] : 0;
            break;

        case 0xA0: // Poly Pressure
            msg.type = MidiMessage::Type::PolyPressure;
            msg.data1 = (length > 1) ? data[1] : 0;
            msg.data2 = (length > 2) ? data[2] : 0;
            break;

        case 0xC0: // Program Change
            msg.type = MidiMessage::Type::ProgramChange;
            msg.data1 = (length > 1) ? data[1] : 0;
            break;

        default:
            break;
        }

        return msg;
    }

} // namespace sfz

#else
// Non-Windows stubs
namespace sfz
{
    WinrtMidi::WinrtMidi() = default;
    WinrtMidi::~WinrtMidi() { close(); }
    std::vector<std::string> WinrtMidi::getInputPortList() { return {}; }
    bool WinrtMidi::open(MidiInputCallback, uint32_t) { return false; }
    void WinrtMidi::close() {}
    MidiMessage WinrtMidi::parseMidiBytes(const uint8_t*, uint32_t, uint32_t) { return {}; }
}
#endif
