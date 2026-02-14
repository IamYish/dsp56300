#pragma once

#include "sample/sampleData.h"

#include <string>
#include <memory>

namespace sfz
{
    // WAV file loader.
    // Supports:
    //   - PCM 8/16/24/32-bit
    //   - IEEE float 32/64-bit
    //   - Mono and stereo (multi-channel)
    //   - Loop points from the 'smpl' chunk
    //   - Standard RIFF WAVE format
    class WavLoader
    {
    public:
        // Load a WAV file from disk.
        // Returns nullptr on failure.
        static std::unique_ptr<SampleData> load(const std::string& filePath);

    private:
        // RIFF chunk IDs
        static constexpr uint32_t RIFF_ID = 0x46464952; // 'RIFF'
        static constexpr uint32_t WAVE_ID = 0x45564157; // 'WAVE'
        static constexpr uint32_t FMT_ID  = 0x20746D66; // 'fmt '
        static constexpr uint32_t DATA_ID = 0x61746164; // 'data'
        static constexpr uint32_t SMPL_ID = 0x6C706D73; // 'smpl'

        // WAV format tags
        static constexpr uint16_t WAVE_FORMAT_PCM        = 0x0001;
        static constexpr uint16_t WAVE_FORMAT_IEEE_FLOAT  = 0x0003;
        static constexpr uint16_t WAVE_FORMAT_EXTENSIBLE  = 0xFFFE;

#pragma pack(push, 1)
        struct RiffHeader
        {
            uint32_t chunkId;       // 'RIFF'
            uint32_t chunkSize;
            uint32_t format;        // 'WAVE'
        };

        struct FmtChunk
        {
            uint16_t audioFormat;
            uint16_t numChannels;
            uint32_t sampleRate;
            uint32_t byteRate;
            uint16_t blockAlign;
            uint16_t bitsPerSample;
        };

        struct SmplLoop
        {
            uint32_t id;
            uint32_t type;
            uint32_t start;
            uint32_t end;
            uint32_t fraction;
            uint32_t playCount;
        };
#pragma pack(pop)
    };

} // namespace sfz
