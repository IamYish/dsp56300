#include "wavLoader.h"

#include <fstream>
#include <cstring>
#include <cmath>

namespace sfz
{
    std::unique_ptr<SampleData> WavLoader::load(const std::string& filePath)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
            return nullptr;

        // Read RIFF header
        RiffHeader riff{};
        file.read(reinterpret_cast<char*>(&riff), sizeof(riff));

        if (riff.chunkId != RIFF_ID || riff.format != WAVE_ID)
            return nullptr;

        // Scan chunks
        FmtChunk fmt{};
        bool fmtFound = false;
        std::vector<uint8_t> rawData;
        uint32_t loopStart = 0;
        uint32_t loopEnd = 0;
        bool hasLoop = false;

        while (file.good())
        {
            uint32_t chunkId = 0;
            uint32_t chunkSize = 0;

            file.read(reinterpret_cast<char*>(&chunkId), 4);
            file.read(reinterpret_cast<char*>(&chunkSize), 4);

            if (!file.good())
                break;

            const auto chunkStart = file.tellg();

            if (chunkId == FMT_ID)
            {
                file.read(reinterpret_cast<char*>(&fmt), sizeof(fmt));
                fmtFound = true;

                // Handle WAVE_FORMAT_EXTENSIBLE: the actual format is in the
                // sub-format GUID. First two bytes of the GUID are the format tag.
                if (fmt.audioFormat == WAVE_FORMAT_EXTENSIBLE && chunkSize >= 40)
                {
                    // Skip cbSize (2 bytes) + validBitsPerSample (2 bytes) + channelMask (4 bytes)
                    uint16_t cbSize = 0;
                    file.read(reinterpret_cast<char*>(&cbSize), 2);
                    file.seekg(6, std::ios::cur); // skip validBitsPerSample + channelMask

                    uint16_t subFormat = 0;
                    file.read(reinterpret_cast<char*>(&subFormat), 2);
                    fmt.audioFormat = subFormat;
                }
            }
            else if (chunkId == DATA_ID)
            {
                rawData.resize(chunkSize);
                file.read(reinterpret_cast<char*>(rawData.data()), chunkSize);
            }
            else if (chunkId == SMPL_ID)
            {
                // Parse sampler chunk for loop points
                // Skip manufacturer, product, samplePeriod, midiUnityNote,
                // midiPitchFraction, smpteFormat, smpteOffset (28 bytes)
                file.seekg(28, std::ios::cur);

                uint32_t numLoops = 0;
                file.read(reinterpret_cast<char*>(&numLoops), 4);

                // Skip samplerData
                file.seekg(4, std::ios::cur);

                if (numLoops > 0)
                {
                    SmplLoop loop{};
                    file.read(reinterpret_cast<char*>(&loop), sizeof(loop));
                    loopStart = loop.start;
                    loopEnd = loop.end;
                    hasLoop = true;
                }
            }

            // Seek to next chunk (chunks are word-aligned)
            auto nextPos = static_cast<std::streamoff>(chunkStart) + chunkSize;
            if (chunkSize & 1) ++nextPos; // padding byte
            file.seekg(nextPos);
        }

        if (!fmtFound || rawData.empty())
            return nullptr;

        // Validate format
        if (fmt.audioFormat != WAVE_FORMAT_PCM && fmt.audioFormat != WAVE_FORMAT_IEEE_FLOAT)
            return nullptr;

        const uint32_t channels = fmt.numChannels;
        const SampleRate sampleRate = fmt.sampleRate;
        const uint16_t bitsPerSample = fmt.bitsPerSample;

        // Convert raw data to float
        const uint32_t bytesPerSample = bitsPerSample / 8;
        const uint32_t totalSamples = static_cast<uint32_t>(rawData.size()) / bytesPerSample;
        const uint32_t frameCount = totalSamples / channels;

        std::vector<float> floatData(totalSamples);

        if (fmt.audioFormat == WAVE_FORMAT_IEEE_FLOAT)
        {
            if (bitsPerSample == 32)
            {
                std::memcpy(floatData.data(), rawData.data(), rawData.size());
            }
            else if (bitsPerSample == 64)
            {
                const auto* src = reinterpret_cast<const double*>(rawData.data());
                for (uint32_t i = 0; i < totalSamples; ++i)
                    floatData[i] = static_cast<float>(src[i]);
            }
        }
        else // PCM
        {
            switch (bitsPerSample)
            {
            case 8:
                for (uint32_t i = 0; i < totalSamples; ++i)
                    floatData[i] = (static_cast<float>(rawData[i]) - 128.0f) / 128.0f;
                break;

            case 16:
            {
                const auto* src = reinterpret_cast<const int16_t*>(rawData.data());
                constexpr float scale = 1.0f / 32768.0f;
                for (uint32_t i = 0; i < totalSamples; ++i)
                    floatData[i] = static_cast<float>(src[i]) * scale;
                break;
            }

            case 24:
            {
                constexpr float scale = 1.0f / 8388608.0f;
                for (uint32_t i = 0; i < totalSamples; ++i)
                {
                    const uint32_t byteOffset = i * 3;
                    int32_t sample = static_cast<int32_t>(rawData[byteOffset])
                                   | (static_cast<int32_t>(rawData[byteOffset + 1]) << 8)
                                   | (static_cast<int32_t>(rawData[byteOffset + 2]) << 16);
                    // Sign extend
                    if (sample & 0x800000)
                        sample |= 0xFF000000;
                    floatData[i] = static_cast<float>(sample) * scale;
                }
                break;
            }

            case 32:
            {
                const auto* src = reinterpret_cast<const int32_t*>(rawData.data());
                constexpr float scale = 1.0f / 2147483648.0f;
                for (uint32_t i = 0; i < totalSamples; ++i)
                    floatData[i] = static_cast<float>(src[i]) * scale;
                break;
            }

            default:
                return nullptr; // unsupported bit depth
            }
        }

        if (!hasLoop)
        {
            loopStart = 0;
            loopEnd = 0;
        }

        return std::make_unique<SampleData>(
            std::move(floatData), channels, sampleRate, loopStart, loopEnd);
    }

} // namespace sfz
