#pragma once

#include "engine/engine.h"

#include <string>
#include <functional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace sfz
{
    // Dear ImGui-based GUI for the standalone sampler.
    //
    // Uses a D3D11 backend for rendering on Windows.
    // The GUI provides:
    //   - SFZ file browser and loader
    //   - Audio/MIDI device selection
    //   - Real-time voice count and CPU meter
    //   - Region/instrument inspector
    //   - CC monitor
    //   - MIDI activity indicator
    //
    // ARCHITECTURE NOTE:
    // Dear ImGui is immediate-mode: the entire UI is rebuilt every frame.
    // The GUI class holds references to the engine and platform backends,
    // and renders the UI by querying their state each frame.
    //
    // ImGui will be added as a git submodule under source/imgui/.
    // Until then, this file defines the GUI interface and rendering loop,
    // with the actual ImGui calls wrapped in #ifdef HAS_IMGUI guards.
    class Gui
    {
    public:
        Gui();
        ~Gui();

        // Non-copyable
        Gui(const Gui&) = delete;
        Gui& operator=(const Gui&) = delete;

        // Initialize the GUI window and rendering context.
        // Returns true on success.
        bool init(const std::string& title, int width, int height);

        // Run the GUI event loop (blocks until window is closed).
        // audioDevices / midiDevices: available device names for selection.
        // onLoadSfz: called when user selects an SFZ file.
        // onAudioDeviceChange: called when user changes audio output device.
        // onMidiDeviceChange: called when user changes MIDI input port.
        void run(Engine& engine,
                 const std::vector<std::string>& audioDevices,
                 const std::vector<std::string>& midiDevices,
                 std::function<void(const std::string&)> onLoadSfz,
                 std::function<void(int)> onAudioDeviceChange,
                 std::function<void(int)> onMidiDeviceChange);

        // Shutdown and destroy the window.
        void shutdown();

        // Check if the window is still open.
        bool isRunning() const { return m_running; }

    private:
        // Render one frame of the UI.
        void renderFrame(Engine& engine);

        // Draw the main menu bar.
        void drawMenuBar();

        // Draw the instrument info panel.
        void drawInstrumentPanel(const Engine& engine);

        // Draw the audio/MIDI settings panel.
        void drawSettingsPanel(const std::vector<std::string>& audioDevices,
                               const std::vector<std::string>& midiDevices);

        // Draw the real-time monitoring panel (voices, CPU, memory).
        void drawMonitorPanel(const Engine& engine);

        // Draw the parser messages / log panel.
        void drawLogPanel(const Engine& engine);

#ifdef _WIN32
        HWND    m_hwnd      = nullptr;
        // D3D11 device, swap chain, etc. will be added with ImGui integration
#endif
        bool    m_running   = false;
        int     m_width     = 1280;
        int     m_height    = 720;

        // UI state
        std::string     m_sfzPath;
        int             m_selectedAudioDevice = 0;
        int             m_selectedMidiDevice  = 3; // Default: port 3
        bool            m_showSettings  = false;
        bool            m_showLog       = true;

        // Callbacks (stored during run())
        std::function<void(const std::string&)> m_onLoadSfz;
        std::function<void(int)>                m_onAudioDeviceChange;
        std::function<void(int)>                m_onMidiDeviceChange;
    };

} // namespace sfz
