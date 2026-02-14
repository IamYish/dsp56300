#pragma once

#include "sfz/region.h"
#include "sfz/curveDefinition.h"

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace sfz
{
    // Represents a fully parsed SFZ instrument.
    // After parsing, the hierarchy (control/global/master/group/region) is
    // flattened: every region is self-contained with all inherited opcodes resolved.
    struct Instrument
    {
        // ---- Metadata -------------------------------------------------------
        std::string     filePath;       // path to the .sfz file
        std::string     defaultPath;    // <control> default_path

        // ---- Global settings (<control> header) -----------------------------
        int             noteOffset  = 0;
        int             octaveOffset = 0;
        std::array<float, Config::MaxCCs>   defaultCC{};    // set_ccN
        std::map<int, std::string>          ccLabels;       // label_ccN

        // ---- Macros (#define) -----------------------------------------------
        std::map<std::string, std::string>  macros;

        // ---- Flattened regions (all inheritance resolved) --------------------
        std::vector<Region>     regions;

        // ---- Custom curves --------------------------------------------------
        std::vector<CurveDefinition>    curves;

        // Built-in curves (indices 0-6) are always present
        void initBuiltinCurves()
        {
            curves.clear();
            curves.push_back(CurveDefinition::makeLinear());
            curves.push_back(CurveDefinition::makeBipolar());
            curves.push_back(CurveDefinition::makeLinearInverted());
            curves.push_back(CurveDefinition::makeBipolarInverted());
            curves.push_back(CurveDefinition::makeConcave());
            curves.push_back(CurveDefinition::makeXfinPower());
            curves.push_back(CurveDefinition::makeXfoutPower());
        }

        // ---- Effects --------------------------------------------------------
        std::vector<EffectParams>   effects;

        // ---- Lookup helpers -------------------------------------------------

        // Find the curve with the given index, or nullptr if not found.
        const CurveDefinition* findCurve(int index) const
        {
            for (const auto& c : curves)
            {
                if (c.getIndex() == index)
                    return &c;
            }
            return nullptr;
        }

        // Get all regions that match a given MIDI note and velocity.
        // This is the core lookup for note-on events.
        std::vector<const Region*> findRegions(MidiNote note, MidiVel velocity,
                                                MidiChannel channel) const
        {
            std::vector<const Region*> result;
            for (const auto& r : regions)
            {
                if (note >= r.keyRange.lo && note <= r.keyRange.hi &&
                    velocity >= r.velRange.lo && velocity <= r.velRange.hi &&
                    channel >= r.loChan && channel <= r.hiChan)
                {
                    result.push_back(&r);
                }
            }
            return result;
        }
    };

} // namespace sfz
