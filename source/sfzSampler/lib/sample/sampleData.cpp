#include "sampleData.h"

namespace sfz
{
    SampleData::SampleData(std::vector<float>&& data, uint32_t channels, SampleRate sampleRate,
                           uint32_t loopStart, uint32_t loopEnd)
        : m_data(std::move(data))
        , m_channels(channels)
        , m_sampleRate(sampleRate)
        , m_loopStart(loopStart)
        , m_loopEnd(loopEnd)
    {
        if (m_channels > 0)
            m_frameCount = static_cast<uint32_t>(m_data.size()) / m_channels;
    }

} // namespace sfz
