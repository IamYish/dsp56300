#include "gui.h"

#ifdef _WIN32
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#endif

// Dear ImGui integration:
// Once imgui is added as a submodule, uncomment these includes:
// #include "imgui.h"
// #include "imgui_impl_win32.h"
// #include "imgui_impl_dx11.h"
// #define HAS_IMGUI 1

namespace sfz
{
    Gui::Gui() = default;
    Gui::~Gui() { shutdown(); }

#ifdef _WIN32

    // Win32 window procedure
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // Forward to ImGui when integrated:
        // extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
        // if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        //     return true;

        switch (msg)
        {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            // Handle resize (recreate swap chain render target)
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    bool Gui::init(const std::string& title, int width, int height)
    {
        m_width = width;
        m_height = height;

        // Register window class
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_CLASSDC;
        wc.lpfnWndProc = wndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"SfzSamplerClass";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassExW(&wc);

        // Convert title to wide string
        wchar_t wTitle[256]{};
        MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, wTitle, 256);

        // Create window
        m_hwnd = CreateWindowExW(
            0, wc.lpszClassName, wTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, width, height,
            nullptr, nullptr, wc.hInstance, nullptr);

        if (!m_hwnd)
            return false;

        ShowWindow(m_hwnd, SW_SHOWDEFAULT);
        UpdateWindow(m_hwnd);

        // TODO: Initialize D3D11 device, swap chain, and ImGui context
        // This will be done when Dear ImGui is added as a dependency:
        //
        // 1. Create D3D11 device and swap chain
        // 2. ImGui::CreateContext()
        // 3. ImGui_ImplWin32_Init(m_hwnd)
        // 4. ImGui_ImplDX11_Init(device, deviceContext)
        // 5. Configure ImGui style (dark theme)

        m_running = true;
        return true;
    }

    void Gui::run(Engine& engine,
                  const std::vector<std::string>& audioDevices,
                  const std::vector<std::string>& midiDevices,
                  std::function<void(const std::string&)> onLoadSfz,
                  std::function<void(int)> onAudioDeviceChange,
                  std::function<void(int)> onMidiDeviceChange)
    {
        m_onLoadSfz = std::move(onLoadSfz);
        m_onAudioDeviceChange = std::move(onAudioDeviceChange);
        m_onMidiDeviceChange = std::move(onMidiDeviceChange);

        MSG msg{};
        while (m_running)
        {
            // Process Windows messages
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    m_running = false;
                    break;
                }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }

            if (!m_running)
                break;

            // Render frame
            renderFrame(engine);

            // Present (swap buffers)
            // TODO: SwapChain->Present(1, 0) when D3D11 is initialized

            // Simple throttle (will be replaced by vsync when D3D11 is active)
            Sleep(16); // ~60 FPS
        }
    }

    void Gui::renderFrame(Engine& engine)
    {
        // When ImGui is integrated, this will be:
        //
        // ImGui_ImplDX11_NewFrame();
        // ImGui_ImplWin32_NewFrame();
        // ImGui::NewFrame();
        //
        // drawMenuBar();
        // drawInstrumentPanel(engine);
        // drawMonitorPanel(engine);
        // drawSettingsPanel(audioDevices, midiDevices);
        // if (m_showLog)
        //     drawLogPanel(engine);
        //
        // ImGui::Render();
        // Clear render target and draw ImGui
        //
        // For now, just prevent unused parameter warnings:
        (void)engine;
    }

    void Gui::drawMenuBar()
    {
#ifdef HAS_IMGUI
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open SFZ..."))
                {
                    // Open file dialog
                    OPENFILENAMEW ofn{};
                    wchar_t filePath[MAX_PATH]{};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = m_hwnd;
                    ofn.lpstrFilter = L"SFZ Files\0*.sfz\0All Files\0*.*\0";
                    ofn.lpstrFile = filePath;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                    ofn.lpstrDefExt = L"sfz";

                    if (GetOpenFileNameW(&ofn) && m_onLoadSfz)
                    {
                        char narrowPath[MAX_PATH]{};
                        WideCharToMultiByte(CP_UTF8, 0, filePath, -1, narrowPath, MAX_PATH, nullptr, nullptr);
                        m_sfzPath = narrowPath;
                        m_onLoadSfz(m_sfzPath);
                    }
                }
                if (ImGui::MenuItem("Quit"))
                    m_running = false;

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Settings", nullptr, &m_showSettings);
                ImGui::MenuItem("Log", nullptr, &m_showLog);
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }
#endif
    }

    void Gui::drawInstrumentPanel(const Engine& engine)
    {
#ifdef HAS_IMGUI
        ImGui::Begin("Instrument");

        if (engine.isInstrumentLoaded())
        {
            const auto* inst = engine.getInstrument();
            ImGui::Text("File: %s", inst->filePath.c_str());
            ImGui::Text("Regions: %d", static_cast<int>(inst->regions.size()));
            ImGui::Text("Effects: %d", static_cast<int>(inst->effects.size()));
            ImGui::Text("Curves: %d", static_cast<int>(inst->curves.size()));
            ImGui::Separator();
            ImGui::Text("Sample Memory: %.1f MB",
                         static_cast<float>(engine.getSampleMemoryUsage()) / (1024.0f * 1024.0f));
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                               "No instrument loaded. Use File > Open SFZ...");
        }

        ImGui::End();
#else
        (void)engine;
#endif
    }

    void Gui::drawSettingsPanel(const std::vector<std::string>& audioDevices,
                                 const std::vector<std::string>& midiDevices)
    {
#ifdef HAS_IMGUI
        if (!m_showSettings)
            return;

        ImGui::Begin("Settings", &m_showSettings);

        // Audio device selection
        ImGui::Text("Audio Output:");
        if (ImGui::BeginCombo("##audiodev",
                              (m_selectedAudioDevice < static_cast<int>(audioDevices.size()))
                              ? audioDevices[m_selectedAudioDevice].c_str()
                              : "Default"))
        {
            for (int i = 0; i < static_cast<int>(audioDevices.size()); ++i)
            {
                bool selected = (i == m_selectedAudioDevice);
                if (ImGui::Selectable(audioDevices[i].c_str(), selected))
                {
                    m_selectedAudioDevice = i;
                    if (m_onAudioDeviceChange)
                        m_onAudioDeviceChange(i);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        // MIDI device selection
        ImGui::Text("MIDI Input:");
        if (ImGui::BeginCombo("##mididev",
                              (m_selectedMidiDevice < static_cast<int>(midiDevices.size()))
                              ? midiDevices[m_selectedMidiDevice].c_str()
                              : "None"))
        {
            for (int i = 0; i < static_cast<int>(midiDevices.size()); ++i)
            {
                bool selected = (i == m_selectedMidiDevice);
                if (ImGui::Selectable(midiDevices[i].c_str(), selected))
                {
                    m_selectedMidiDevice = i;
                    if (m_onMidiDeviceChange)
                        m_onMidiDeviceChange(i);
                }
            }
            ImGui::EndCombo();
        }

        ImGui::End();
#else
        (void)audioDevices;
        (void)midiDevices;
#endif
    }

    void Gui::drawMonitorPanel(const Engine& engine)
    {
#ifdef HAS_IMGUI
        ImGui::Begin("Monitor");

        ImGui::Text("Active Voices: %d / %d",
                     engine.getActiveVoiceCount(), Config::MaxVoices);

        // Voice count bar
        float voiceRatio = static_cast<float>(engine.getActiveVoiceCount()) /
                           static_cast<float>(Config::MaxVoices);
        ImGui::ProgressBar(voiceRatio, ImVec2(-1, 0), "");

        ImGui::Separator();

        ImGui::Text("Sample Rate: %u Hz", engine.getSampleRate());
        ImGui::Text("Buffer Size: %u samples", engine.getBufferSize());
        ImGui::Text("Latency: %.1f ms",
                     1000.0f * static_cast<float>(engine.getBufferSize()) /
                     static_cast<float>(engine.getSampleRate()));

        ImGui::End();
#else
        (void)engine;
#endif
    }

    void Gui::drawLogPanel(const Engine& engine)
    {
#ifdef HAS_IMGUI
        if (!m_showLog)
            return;

        ImGui::Begin("Log", &m_showLog);

        for (const auto& msg : engine.getParserMessages())
            ImGui::TextWrapped("%s", msg.c_str());

        // Auto-scroll to bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::End();
#else
        (void)engine;
#endif
    }

    void Gui::shutdown()
    {
        // TODO: Cleanup ImGui and D3D11 when integrated:
        // ImGui_ImplDX11_Shutdown();
        // ImGui_ImplWin32_Shutdown();
        // ImGui::DestroyContext();
        // Release D3D11 resources

        if (m_hwnd)
        {
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
        }

        UnregisterClassW(L"SfzSamplerClass", GetModuleHandleW(nullptr));
        m_running = false;
    }

#else
    // Non-Windows stubs
    bool Gui::init(const std::string&, int, int) { return false; }
    void Gui::run(Engine&, const std::vector<std::string>&, const std::vector<std::string>&,
                  std::function<void(const std::string&)>,
                  std::function<void(int)>,
                  std::function<void(int)>) {}
    void Gui::renderFrame(Engine&) {}
    void Gui::drawMenuBar() {}
    void Gui::drawInstrumentPanel(const Engine&) {}
    void Gui::drawSettingsPanel(const std::vector<std::string>&, const std::vector<std::string>&) {}
    void Gui::drawMonitorPanel(const Engine&) {}
    void Gui::drawLogPanel(const Engine&) {}
    void Gui::shutdown() {}
#endif

} // namespace sfz
