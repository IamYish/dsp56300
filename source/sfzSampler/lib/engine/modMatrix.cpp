#include "modMatrix.h"
#include "sfz/instrument.h"

namespace sfz
{
    void ModMatrix::initDefaults()
    {
        m_ccValues.fill(0.0f);

        if (m_instrument)
        {
            for (uint32_t i = 0; i < Config::MaxCCs && i < m_instrument->defaultCC.size(); ++i)
                m_ccValues[i] = m_instrument->defaultCC[i] / 127.0f;
        }
    }

    float ModMatrix::applyCurve(float normalizedInput, int curveIndex) const
    {
        if (!m_instrument)
            return normalizedInput;

        const auto* curve = m_instrument->findCurve(curveIndex);
        if (!curve)
            return normalizedInput;

        return curve->evaluate(normalizedInput);
    }

} // namespace sfz
