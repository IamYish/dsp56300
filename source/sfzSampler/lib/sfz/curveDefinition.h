#pragma once

#include "core/config.h"

#include <array>
#include <optional>
#include <cmath>

namespace sfz
{
    // A custom curve definition.
    // Supports up to 128 points (v000 - v127).
    // Undefined points are linearly interpolated between defined neighbors.
    class CurveDefinition
    {
    public:
        CurveDefinition() = default;

        void setIndex(int idx) { m_index = idx; }
        int  getIndex() const  { return m_index; }

        void setPoint(int position, float value)
        {
            if (position >= 0 && position < static_cast<int>(Config::CurvePoints))
            {
                m_points[position] = value;
                m_defined[position] = true;
                m_needsInterpolation = true;
            }
        }

        // Build the full curve by linearly interpolating between defined points.
        void interpolate()
        {
            if (!m_needsInterpolation)
                return;

            // Find first and last defined points
            int first = -1, last = -1;
            for (int i = 0; i < static_cast<int>(Config::CurvePoints); ++i)
            {
                if (m_defined[i])
                {
                    if (first < 0) first = i;
                    last = i;
                }
            }

            if (first < 0)
                return; // no points defined

            // Extrapolate before first defined point
            for (int i = 0; i < first; ++i)
                m_points[i] = m_points[first];

            // Extrapolate after last defined point
            for (int i = last + 1; i < static_cast<int>(Config::CurvePoints); ++i)
                m_points[i] = m_points[last];

            // Interpolate between defined points
            int prevDefined = first;
            for (int i = first + 1; i <= last; ++i)
            {
                if (m_defined[i])
                {
                    // Interpolate between prevDefined and i
                    const float startVal = m_points[prevDefined];
                    const float endVal   = m_points[i];
                    const float span     = static_cast<float>(i - prevDefined);

                    for (int j = prevDefined + 1; j < i; ++j)
                    {
                        const float t = static_cast<float>(j - prevDefined) / span;
                        m_points[j] = startVal + t * (endVal - startVal);
                    }
                    prevDefined = i;
                }
            }

            m_needsInterpolation = false;
        }

        // Evaluate the curve at a normalized position (0.0 - 1.0).
        // Maps to the 128-point table.
        float evaluate(float normalizedInput) const
        {
            const float idx = normalizedInput * 127.0f;
            const int lo = static_cast<int>(idx);
            const int hi = lo + 1;
            const float frac = idx - static_cast<float>(lo);

            if (lo < 0) return m_points[0];
            if (hi >= static_cast<int>(Config::CurvePoints)) return m_points[Config::CurvePoints - 1];

            return m_points[lo] + frac * (m_points[hi] - m_points[lo]);
        }

        // Built-in curve generators
        static CurveDefinition makeLinear()         // curve 0: 0 to 1
        {
            CurveDefinition c;
            c.m_index = 0;
            for (int i = 0; i < static_cast<int>(Config::CurvePoints); ++i)
                c.m_points[i] = static_cast<float>(i) / 127.0f;
            return c;
        }

        static CurveDefinition makeBipolar()        // curve 1: -1 to 1
        {
            CurveDefinition c;
            c.m_index = 1;
            for (int i = 0; i < static_cast<int>(Config::CurvePoints); ++i)
                c.m_points[i] = (static_cast<float>(i) / 127.0f) * 2.0f - 1.0f;
            return c;
        }

        static CurveDefinition makeLinearInverted() // curve 2: 1 to 0
        {
            CurveDefinition c;
            c.m_index = 2;
            for (int i = 0; i < static_cast<int>(Config::CurvePoints); ++i)
                c.m_points[i] = 1.0f - (static_cast<float>(i) / 127.0f);
            return c;
        }

        static CurveDefinition makeBipolarInverted() // curve 3: 1 to -1
        {
            CurveDefinition c;
            c.m_index = 3;
            for (int i = 0; i < static_cast<int>(Config::CurvePoints); ++i)
                c.m_points[i] = 1.0f - (static_cast<float>(i) / 127.0f) * 2.0f;
            return c;
        }

        static CurveDefinition makeConcave()        // curve 4: concave (dB-like)
        {
            CurveDefinition c;
            c.m_index = 4;
            for (int i = 0; i < static_cast<int>(Config::CurvePoints); ++i)
            {
                const float x = static_cast<float>(i) / 127.0f;
                // Attempt to match the SFZ concave curve (used by CC7, amp_veltrack)
                if (i == 0)
                    c.m_points[i] = 0.0f;
                else
                    c.m_points[i] = 1.0f - (20.0f / 96.0f) * std::log10((127.0f - static_cast<float>(i - 1)) / 127.0f + 0.0000001f);
            }
            // Clamp
            for (auto& v : c.m_points)
                v = std::max(0.0f, std::min(1.0f, v));
            c.m_points[127] = 1.0f;
            return c;
        }

        static CurveDefinition makeXfinPower()      // curve 5: crossfade in (power)
        {
            CurveDefinition c;
            c.m_index = 5;
            for (int i = 0; i < static_cast<int>(Config::CurvePoints); ++i)
            {
                const float x = static_cast<float>(i) / 127.0f;
                c.m_points[i] = std::sqrt(x);
            }
            return c;
        }

        static CurveDefinition makeXfoutPower()     // curve 6: crossfade out (power)
        {
            CurveDefinition c;
            c.m_index = 6;
            for (int i = 0; i < static_cast<int>(Config::CurvePoints); ++i)
            {
                const float x = static_cast<float>(i) / 127.0f;
                c.m_points[i] = std::sqrt(1.0f - x);
            }
            return c;
        }

    private:
        int     m_index = 0;
        bool    m_needsInterpolation = false;
        std::array<float, Config::CurvePoints>  m_points{};
        std::array<bool, Config::CurvePoints>   m_defined{};
    };

} // namespace sfz
