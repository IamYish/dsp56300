#pragma once

#include <cstdint>

namespace sfz
{
    // Engine-wide compile-time constants
    struct Config
    {
        // Audio
        static constexpr uint32_t   DefaultSampleRate       = 44100;
        static constexpr uint32_t   DefaultBufferSize       = 2048;
        static constexpr uint32_t   MaxChannels             = 2;        // Stereo

        // Voice management
        static constexpr uint32_t   MaxVoices               = 256;
        static constexpr uint32_t   DefaultPolyphony        = 64;

        // SFZ limits
        static constexpr uint32_t   MaxCCs                  = 138;      // 0-137 (extended)
        static constexpr uint32_t   MaxFlexEGs              = 32;
        static constexpr uint32_t   MaxFlexLFOs             = 32;
        static constexpr uint32_t   MaxEGPoints             = 64;
        static constexpr uint32_t   MaxEQBands              = 3;
        static constexpr uint32_t   MaxFilters              = 2;
        static constexpr uint32_t   MaxEffectBuses          = 8;
        static constexpr uint32_t   MaxCurves               = 256;
        static constexpr uint32_t   CurvePoints             = 128;      // v000-v127

        // MIDI defaults
        static constexpr uint32_t   DefaultMidiPort         = 3;        // User-specified default

        // WinMM defaults
        static constexpr uint32_t   WinmmDefaultBufferSize  = 2048;
        static constexpr uint32_t   WinmmNumBuffers         = 2;        // Double-buffering
    };

} // namespace sfz
