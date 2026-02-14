#include "eq.h"

#include <cmath>

namespace sfz
{
    void ParametricEQ::configure(const std::array<EQBandParams, Config::MaxEQBands>& bands,
                                 float velocity, SampleRate sampleRate)
    {
        m_sampleRate = sampleRate;
        const float velNorm = velocity / 127.0f;

        for (int i = 0; i < Config::MaxEQBands; ++i)
        {
            const auto& src = bands[i];
            auto& dst = m_bands[i];

            dst.freq = src.freq.base + src.vel2freq * velNorm;
            dst.bw = src.bw.base;
            dst.gain = src.gain.base + src.vel2gain * velNorm;
            dst.type = src.type;

            // Band is active if it has a non-zero frequency and non-zero gain (or is a shelf)
            dst.active = (dst.freq > 0.0f) &&
                         (std::abs(dst.gain) > 0.01f || dst.type != EqType::Peak);

            if (dst.active)
            {
                // Map EQ type to filter type
                FilterType ftype;
                switch (dst.type)
                {
                case EqType::LowShelf:  ftype = FilterType::LSH; break;
                case EqType::HighShelf:  ftype = FilterType::HSH; break;
                default:                 ftype = FilterType::PKF_2P; break;
                }

                // For peaking/shelf filters, resonance = gain
                dst.filter.configure(ftype, dst.freq, dst.gain, sampleRate);
            }
        }
    }

    void ParametricEQ::updateBand(int band, float freqHz, float /*bwOctaves*/, float gainDb)
    {
        if (band < 0 || band >= Config::MaxEQBands)
            return;

        auto& b = m_bands[band];
        b.freq = freqHz;
        b.gain = gainDb;
        b.active = (b.freq > 0.0f) && (std::abs(b.gain) > 0.01f || b.type != EqType::Peak);

        if (b.active)
            b.filter.updateParameters(b.freq, b.gain);
    }

    float ParametricEQ::process(float input)
    {
        float out = input;
        for (auto& band : m_bands)
        {
            if (band.active)
                out = band.filter.process(out);
        }
        return out;
    }

    void ParametricEQ::reset()
    {
        for (auto& band : m_bands)
            band.filter.reset();
    }

    bool ParametricEQ::isActive() const
    {
        for (const auto& band : m_bands)
        {
            if (band.active)
                return true;
        }
        return false;
    }

} // namespace sfz
