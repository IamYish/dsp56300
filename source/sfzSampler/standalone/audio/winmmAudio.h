#pragma once

#include "core/config.h"
#include "core/types.h"

#include <functional>
#include <string>
#include <vector>
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <mmsystem.h>
#endif

namespace sfz
{
    // Audio callback: called from audio thread to fill a buffer.
    // Signature: void(float* outputL, float* outputR, uint32_t numFrames)
    using AudioCallback = std::function<void(float* outputL, float* outputR, uint32_t numFrames)>;

    // WinMM waveOut audio output backend.
    //
    // Provides double-buffered audio output using the Windows Multimedia
    // waveOut API. Audio is rendered at 16-bit PCM via waveOutWrite.
    //
    // Default configuration:
    //   - 44100 Hz stereo
    //   - 2048 sample buffer
    //   - Double buffering
    //
    // The audio callback runs on a dedicated thread, signaled by WOM_DONE events.
    class WinmmAudio
    {
    public:
        WinmmAudio();
        ~WinmmAudio();

        // Non-copyable
        WinmmAudio(const WinmmAudio&) = delete;
        WinmmAudio& operator=(const WinmmAudio&) = delete;

        // List available output devices.
        static std::vector<std::string> getDeviceList();

        // Open and start audio output.
        // deviceId: WAVE_MAPPER (default) or specific device index.
        // Returns true on success.
        bool open(AudioCallback callback,
                  SampleRate sampleRate = Config::DefaultSampleRate,
                  uint32_t bufferSize = Config::WinmmDefaultBufferSize,
                  uint32_t deviceId = 0xFFFFFFFF); // WAVE_MAPPER

        // Stop and close audio output.
        void close();

        // Query state
        bool isOpen() const { return m_isOpen; }
        SampleRate getSampleRate() const { return m_sampleRate; }
        uint32_t getBufferSize() const { return m_bufferSize; }

    private:
#ifdef _WIN32
        // WinMM callback (static, dispatches to instance)
        static void CALLBACK waveOutProc(HWAVEOUT hwo, UINT uMsg,
                                          DWORD_PTR dwInstance,
                                          DWORD_PTR dwParam1,
                                          DWORD_PTR dwParam2);

        // Fill and submit a buffer
        void fillBuffer(int bufferIndex);

        HWAVEOUT    m_hWaveOut  = nullptr;
        HANDLE      m_event     = nullptr;
        HANDLE      m_thread    = nullptr;

        static constexpr int NumBuffers = Config::WinmmNumBuffers;

        struct AudioBuffer
        {
            WAVEHDR             header{};
            std::vector<int16_t> data;
            std::vector<float>  floatL;
            std::vector<float>  floatR;
        };

        AudioBuffer m_buffers[NumBuffers];

        static DWORD WINAPI audioThreadProc(LPVOID param);
        void audioThreadLoop();
#endif

        AudioCallback   m_callback;
        SampleRate      m_sampleRate    = Config::DefaultSampleRate;
        uint32_t        m_bufferSize    = Config::WinmmDefaultBufferSize;
        bool            m_isOpen        = false;
        bool            m_running       = false;
    };

} // namespace sfz
