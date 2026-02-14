#pragma once

#include "sfz/types.h"
#include "sfz/curveDefinition.h"
#include "core/config.h"

#include <array>
#include <cmath>

namespace sfz
{
    class Instrument;

    // Modulation matrix: evaluates CC modulations at runtime.
    // Holds the current CC state and resolves ModulatedFloat values.
    class ModMatrix
    {
    public:
        ModMatrix() = default;

        // Set the instrument reference (for curve lookup).
        void setInstrument(const Instrument* inst) { m_instrument = inst; }

        // Set a CC value (0.0 - 1.0 normalized).
        void setCC(MidiCC cc, float value)
        {
            if (cc < Config::MaxCCs)
                m_ccValues[cc] = value;
        }

        // Get a CC value.
        float getCC(MidiCC cc) const
        {
            return (cc < Config::MaxCCs) ? m_ccValues[cc] : 0.0f;
        }

        // Resolve a ModulatedFloat: base value + all CC modulations applied.
        float resolve(const ModulatedFloat& param) const
        {
            float result = param.base;
            for (const auto& mod : param.mods)
            {
                float ccVal = getCC(mod.cc);

                // Apply curve if specified
                if (mod.curveIndex >= 0 && m_instrument)
                    ccVal = applyCurve(ccVal, mod.curveIndex);

                result += ccVal * mod.depth;
            }
            return result;
        }

        // Resolve a ModulatedInt.
        int resolveInt(const ModulatedInt& param) const
        {
            float result = static_cast<float>(param.base);
            for (const auto& mod : param.mods)
            {
                float ccVal = getCC(mod.cc);
                if (mod.curveIndex >= 0 && m_instrument)
                    ccVal = applyCurve(ccVal, mod.curveIndex);
                result += ccVal * mod.depth;
            }
            return static_cast<int>(std::round(result));
        }

        // Set pitch bend (-8192 to 8191) as extended CC.
        void setPitchBend(int value)
        {
            setCC(CC_PitchBend, static_cast<float>(value + 8192) / 16383.0f);
        }

        // Set channel aftertouch (0-127) as extended CC.
        void setChannelAT(int value)
        {
            setCC(CC_ChannelAT, static_cast<float>(value) / 127.0f);
        }

        // Set poly aftertouch as extended CC.
        void setPolyAT(int value)
        {
            setCC(CC_PolyAT, static_cast<float>(value) / 127.0f);
        }

        // Initialize with default CC values from instrument.
        void initDefaults();

    private:
        float applyCurve(float normalizedInput, int curveIndex) const;

        const Instrument*                   m_instrument = nullptr;
        std::array<float, Config::MaxCCs>   m_ccValues{};
    };

} // namespace sfz
