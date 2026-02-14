// sfzSampler - Standalone SFZ v2 Sampler
//
// Entry point for the Windows standalone application.
// Wires together:
//   - sfzSamplerLib engine (SFZ parsing, sample playback, DSP)
//   - WinMM audio output (2048 buffer, 44100 Hz stereo)
//   - WinRT MIDI input (default port 3)
//   - Dear ImGui GUI (D3D11 backend)

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

#include "engine/engine.h"
#include "audio/winmmAudio.h"
#include "midi/winmmMidi.h"
#include "gui/gui.h"
#include "core/config.h"

#include <string>
#include <vector>

namespace
{
    sfz::Engine         g_engine;
    sfz::WinmmAudio     g_audio;
    sfz::WinrtMidi      g_midi;

    // Audio callback: called from the WinMM audio thread.
    void audioCallback(float* outputL, float* outputR, uint32_t numFrames)
    {
        g_engine.processAudio(outputL, outputR, numFrames);
    }

    // MIDI callback: called from the WinMM/WinRT MIDI thread.
    void midiCallback(const sfz::MidiMessage& msg)
    {
        g_engine.pushMidi(msg);
    }

    // Load an SFZ instrument file.
    void loadSfz(const std::string& path)
    {
        g_engine.loadInstrument(path);
    }

    // Change audio output device.
    void changeAudioDevice(int deviceIndex)
    {
        g_audio.close();
        g_audio.open(audioCallback,
                     sfz::Config::DefaultSampleRate,
                     sfz::Config::WinmmDefaultBufferSize,
                     static_cast<uint32_t>(deviceIndex));
    }

    // Change MIDI input port.
    void changeMidiDevice(int portIndex)
    {
        g_midi.close();
        g_midi.open(midiCallback, static_cast<uint32_t>(portIndex));
    }
}

#ifdef _WIN32

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
                   LPSTR lpCmdLine, int /*nCmdShow*/)
{
    // Initialize COM (required for WinRT MIDI and file dialogs)
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Configure the engine
    g_engine.setSampleRate(sfz::Config::DefaultSampleRate);
    g_engine.setBufferSize(sfz::Config::WinmmDefaultBufferSize);

    // Start audio output
    if (!g_audio.open(audioCallback,
                      sfz::Config::DefaultSampleRate,
                      sfz::Config::WinmmDefaultBufferSize))
    {
        MessageBoxW(nullptr, L"Failed to open audio output device.",
                    L"sfzSampler", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Start MIDI input (default port 3)
    g_midi.open(midiCallback, sfz::Config::DefaultMidiPort);

    // If an SFZ file was passed on the command line, load it
    if (lpCmdLine && lpCmdLine[0] != '\0')
    {
        std::string cmdLine(lpCmdLine);
        // Strip quotes if present
        if (cmdLine.front() == '"' && cmdLine.back() == '"')
            cmdLine = cmdLine.substr(1, cmdLine.size() - 2);
        g_engine.loadInstrument(cmdLine);
    }

    // Enumerate devices for the GUI
    auto audioDevices = sfz::WinmmAudio::getDeviceList();
    auto midiDevices  = sfz::WinrtMidi::getInputPortList();

    // Create and run the GUI
    sfz::Gui gui;
    if (!gui.init("sfzSampler - SFZ v2 Sampler", 1280, 720))
    {
        MessageBoxW(nullptr, L"Failed to create GUI window.",
                    L"sfzSampler", MB_OK | MB_ICONERROR);
        g_audio.close();
        g_midi.close();
        return 1;
    }

    gui.run(g_engine, audioDevices, midiDevices,
            loadSfz, changeAudioDevice, changeMidiDevice);

    // Shutdown
    gui.shutdown();
    g_midi.close();
    g_audio.close();
    g_engine.unloadInstrument();

    CoUninitialize();
    return 0;
}

#else

// Non-Windows entry point (placeholder for future cross-platform support)
int main(int /*argc*/, char** /*argv*/)
{
    return 0;
}

#endif
