#include "winmmAudio.h"

#ifdef _WIN32
#pragma comment(lib, "winmm.lib")
#endif

#include <algorithm>
#include <cstring>

namespace sfz
{
    WinmmAudio::WinmmAudio() = default;

    WinmmAudio::~WinmmAudio()
    {
        close();
    }

#ifdef _WIN32

    std::vector<std::string> WinmmAudio::getDeviceList()
    {
        std::vector<std::string> devices;
        const UINT numDevs = waveOutGetNumDevs();
        for (UINT i = 0; i < numDevs; ++i)
        {
            WAVEOUTCAPSW caps{};
            if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) == MMSYSERR_NOERROR)
            {
                // Convert wide string to narrow
                char name[64]{};
                WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, name, sizeof(name), nullptr, nullptr);
                devices.emplace_back(name);
            }
        }
        return devices;
    }

    bool WinmmAudio::open(AudioCallback callback, SampleRate sampleRate,
                          uint32_t bufferSize, uint32_t deviceId)
    {
        if (m_isOpen)
            close();

        m_callback = std::move(callback);
        m_sampleRate = sampleRate;
        m_bufferSize = bufferSize;

        // Set up wave format (16-bit stereo PCM)
        WAVEFORMATEX wfx{};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 2;
        wfx.nSamplesPerSec = sampleRate;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        wfx.cbSize = 0;

        // Create event for buffer-done signaling
        m_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_event)
            return false;

        // Open the wave device
        MMRESULT result = waveOutOpen(&m_hWaveOut, deviceId, &wfx,
                                       reinterpret_cast<DWORD_PTR>(m_event),
                                       0, CALLBACK_EVENT);
        if (result != MMSYSERR_NOERROR)
        {
            CloseHandle(m_event);
            m_event = nullptr;
            return false;
        }

        // Allocate and prepare buffers
        const uint32_t bufferBytes = bufferSize * wfx.nBlockAlign;

        for (int i = 0; i < NumBuffers; ++i)
        {
            auto& buf = m_buffers[i];
            buf.data.resize(bufferSize * 2); // stereo interleaved int16
            buf.floatL.resize(bufferSize);
            buf.floatR.resize(bufferSize);

            std::memset(&buf.header, 0, sizeof(WAVEHDR));
            buf.header.lpData = reinterpret_cast<LPSTR>(buf.data.data());
            buf.header.dwBufferLength = bufferBytes;

            waveOutPrepareHeader(m_hWaveOut, &buf.header, sizeof(WAVEHDR));

            // Fill initial buffer with silence and submit
            std::memset(buf.data.data(), 0, bufferBytes);
            waveOutWrite(m_hWaveOut, &buf.header, sizeof(WAVEHDR));
        }

        m_isOpen = true;
        m_running = true;

        // Start audio thread
        m_thread = CreateThread(nullptr, 0, audioThreadProc, this, 0, nullptr);
        if (!m_thread)
        {
            close();
            return false;
        }

        // Set thread priority for real-time audio
        SetThreadPriority(m_thread, THREAD_PRIORITY_TIME_CRITICAL);

        return true;
    }

    void WinmmAudio::close()
    {
        m_running = false;

        if (m_event)
            SetEvent(m_event); // wake up the thread

        if (m_thread)
        {
            WaitForSingleObject(m_thread, 5000);
            CloseHandle(m_thread);
            m_thread = nullptr;
        }

        if (m_hWaveOut)
        {
            waveOutReset(m_hWaveOut);

            for (int i = 0; i < NumBuffers; ++i)
                waveOutUnprepareHeader(m_hWaveOut, &m_buffers[i].header, sizeof(WAVEHDR));

            waveOutClose(m_hWaveOut);
            m_hWaveOut = nullptr;
        }

        if (m_event)
        {
            CloseHandle(m_event);
            m_event = nullptr;
        }

        m_isOpen = false;
    }

    void WinmmAudio::fillBuffer(int bufferIndex)
    {
        auto& buf = m_buffers[bufferIndex];

        // Clear float buffers
        std::memset(buf.floatL.data(), 0, m_bufferSize * sizeof(float));
        std::memset(buf.floatR.data(), 0, m_bufferSize * sizeof(float));

        // Call the audio callback to fill float buffers
        if (m_callback)
            m_callback(buf.floatL.data(), buf.floatR.data(), m_bufferSize);

        // Convert float [-1,1] to int16 interleaved
        for (uint32_t i = 0; i < m_bufferSize; ++i)
        {
            float l = std::clamp(buf.floatL[i], -1.0f, 1.0f);
            float r = std::clamp(buf.floatR[i], -1.0f, 1.0f);
            buf.data[i * 2]     = static_cast<int16_t>(l * 32767.0f);
            buf.data[i * 2 + 1] = static_cast<int16_t>(r * 32767.0f);
        }

        // Submit the buffer
        waveOutWrite(m_hWaveOut, &buf.header, sizeof(WAVEHDR));
    }

    DWORD WINAPI WinmmAudio::audioThreadProc(LPVOID param)
    {
        auto* self = static_cast<WinmmAudio*>(param);
        self->audioThreadLoop();
        return 0;
    }

    void WinmmAudio::audioThreadLoop()
    {
        while (m_running)
        {
            // Wait for a buffer to complete
            WaitForSingleObject(m_event, 100); // 100ms timeout for shutdown check

            if (!m_running)
                break;

            // Check which buffers are done and refill them
            for (int i = 0; i < NumBuffers; ++i)
            {
                if (m_buffers[i].header.dwFlags & WHDR_DONE)
                    fillBuffer(i);
            }
        }
    }

    void CALLBACK WinmmAudio::waveOutProc(HWAVEOUT /*hwo*/, UINT /*uMsg*/,
                                           DWORD_PTR /*dwInstance*/,
                                           DWORD_PTR /*dwParam1*/,
                                           DWORD_PTR /*dwParam2*/)
    {
        // Not used — we use CALLBACK_EVENT instead of CALLBACK_FUNCTION
    }

#else
    // Non-Windows stubs
    std::vector<std::string> WinmmAudio::getDeviceList() { return {}; }
    bool WinmmAudio::open(AudioCallback, SampleRate, uint32_t, uint32_t) { return false; }
    void WinmmAudio::close() {}
#endif

} // namespace sfz
