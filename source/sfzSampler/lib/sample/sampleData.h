#pragma once

#include "core/types.h"

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

namespace sfz
{
    // Holds decoded audio sample data in memory.
    // All samples are stored as interleaved 32-bit float, normalized to [-1, 1].
    class SampleData
    {
    public:
        SampleData() = default;

        // Construct with pre-allocated data
        SampleData(std::vector<float>&& data, uint32_t channels, SampleRate sampleRate,
                   uint32_t loopStart = 0, uint32_t loopEnd = 0);

        // Accessors
        const float*    getData() const             { return m_data.data(); }
        float*          getData()                   { return m_data.data(); }
        size_t          getDataSize() const         { return m_data.size(); }

        uint32_t        getChannels() const         { return m_channels; }
        SampleRate      getSampleRate() const       { return m_sampleRate; }
        uint32_t        getFrameCount() const       { return m_frameCount; }

        // Loop points (from WAV smpl chunk or SFZ opcodes)
        uint32_t        getLoopStart() const        { return m_loopStart; }
        uint32_t        getLoopEnd() const          { return m_loopEnd; }
        bool            hasLoop() const             { return m_loopEnd > m_loopStart; }

        // Read a single sample at frame position and channel
        float getSample(uint32_t frame, uint32_t channel) const
        {
            if (frame >= m_frameCount || channel >= m_channels)
                return 0.0f;
            return m_data[frame * m_channels + channel];
        }

        // Linear interpolation read (for pitch shifting)
        float getSampleInterpolated(double framePos, uint32_t channel) const
        {
            if (m_data.empty())
                return 0.0f;

            const auto idx0 = static_cast<uint32_t>(framePos);
            const auto idx1 = idx0 + 1;
            const float frac = static_cast<float>(framePos - static_cast<double>(idx0));

            const float s0 = getSample(idx0, channel);
            const float s1 = getSample(idx1, channel);
            return s0 + frac * (s1 - s0);
        }

        bool empty() const { return m_data.empty(); }

    private:
        std::vector<float>  m_data;
        uint32_t            m_channels      = 0;
        SampleRate          m_sampleRate    = 0;
        uint32_t            m_frameCount    = 0;
        uint32_t            m_loopStart     = 0;
        uint32_t            m_loopEnd       = 0;
    };

} // namespace sfz
