#pragma once

#include "core/types.h"

#include <string>
#include <map>
#include <unordered_map>

namespace sfz
{
    // Forward declarations
    struct Region;
    struct Group;
    struct Instrument;

    // Key range
    struct KeyRange
    {
        MidiNote lo = 0;
        MidiNote hi = 127;
    };

    // Velocity range
    struct VelRange
    {
        MidiVel lo = 0;
        MidiVel hi = 127;
    };

    // CC range condition
    struct CCRange
    {
        MidiCC  cc = 0;
        float   lo = 0.0f;
        float   hi = 127.0f;
    };

    // DAHDSR envelope definition (used for ampeg, fileg, pitcheg)
    struct EnvelopeParams
    {
        ModulatedFloat  delay   {0.0f};     // seconds
        ModulatedFloat  start   {0.0f};     // 0-100%
        ModulatedFloat  attack  {0.0f};     // seconds
        ModulatedFloat  hold    {0.0f};     // seconds
        ModulatedFloat  decay   {0.0f};     // seconds
        ModulatedFloat  sustain {100.0f};   // 0-100%
        ModulatedFloat  release {0.0f};     // seconds
        float           depth   = 0.0f;     // modulation depth (filter/pitch only)

        // Velocity-to-stage modulation
        float vel2delay     = 0.0f;
        float vel2attack    = 0.0f;
        float vel2hold      = 0.0f;
        float vel2decay     = 0.0f;
        float vel2sustain   = 0.0f;
        float vel2release   = 0.0f;
    };

    // Flex envelope point (SFZ v2 egN_*)
    struct FlexEGPoint
    {
        ModulatedFloat  time    {0.0f};     // seconds from previous point
        ModulatedFloat  level   {0.0f};     // 0.0-1.0
        float           shape   = 0.0f;     // curve shape (0 = linear)
        int             curveIndex = -1;    // custom curve reference
    };

    // Flex envelope definition (SFZ v2 egN_*)
    struct FlexEGParams
    {
        std::vector<FlexEGPoint>    points;
        int                         sustainPoint = -1;  // which point is sustain
        int                         loopPoint    = -1;  // loop start point
        int                         loopCount    = 0;   // loop iterations (0=infinite)

        // Modulation targets and their depths
        float   amplitudeDepth  = 0.0f;
        float   volumeDepth     = 0.0f;
        float   pitchDepth      = 0.0f;
        float   cutoffDepth     = 0.0f;
        float   cutoff2Depth    = 0.0f;
        float   resonanceDepth  = 0.0f;
        float   resonance2Depth = 0.0f;
        float   panDepth        = 0.0f;
        float   widthDepth      = 0.0f;
        bool    isAmpEG         = false;    // egN_ampeg: replaces the amp envelope
    };

    // Fixed LFO definition (amplfo, fillfo, pitchlfo)
    struct FixedLFOParams
    {
        ModulatedFloat  delay   {0.0f};     // seconds
        ModulatedFloat  depth   {0.0f};     // depends on target
        ModulatedFloat  freq    {0.0f};     // Hz
        ModulatedFloat  fade    {0.0f};     // seconds
    };

    // Flex LFO target depth (SFZ v2 lfoN_*)
    struct FlexLFOTarget
    {
        float   volume      = 0.0f;
        float   amplitude   = 0.0f;
        float   pan         = 0.0f;
        float   width       = 0.0f;
        float   position    = 0.0f;
        float   pitch       = 0.0f;
        float   cutoff      = 0.0f;
        float   cutoff2     = 0.0f;
        float   resonance   = 0.0f;
        float   resonance2  = 0.0f;
        // EQ targets per band
        std::array<float, 3>  eqFreq  = {0.0f, 0.0f, 0.0f};
        std::array<float, 3>  eqBw    = {0.0f, 0.0f, 0.0f};
        std::array<float, 3>  eqGain  = {0.0f, 0.0f, 0.0f};
    };

    // Flex LFO definition (SFZ v2 lfoN_*)
    struct FlexLFOParams
    {
        ModulatedFloat  freq    {0.0f};     // Hz
        ModulatedFloat  delay   {0.0f};     // seconds
        ModulatedFloat  fade    {0.0f};     // seconds
        ModulatedFloat  phase   {0.0f};     // 0.0-1.0
        LfoWave         wave    = LfoWave::Triangle;
        int             count   = 0;        // 0 = infinite
        ModulatedFloat  smooth  {0.0f};

        // Step sequencer
        int                 steps = 0;
        std::vector<float>  stepValues;

        // Modulation targets
        FlexLFOTarget       targets;

        // CC modulation on target depths
        std::vector<CCModulation> volumeMods;
        std::vector<CCModulation> pitchMods;
        std::vector<CCModulation> cutoffMods;
    };

    // EQ band parameters
    struct EQBandParams
    {
        ModulatedFloat  freq    {0.0f};
        ModulatedFloat  bw      {1.0f};     // octaves
        ModulatedFloat  gain    {0.0f};     // dB
        EqType          type    = EqType::Peak;
        float           vel2freq = 0.0f;
        float           vel2gain = 0.0f;
    };

    // Filter parameters (one of two serial filters)
    struct FilterParams
    {
        FilterType      type    = FilterType::LPF_2P;
        ModulatedFloat  cutoff  {0.0f};     // Hz (0 = disabled)
        ModulatedFloat  resonance {0.0f};   // dB
        float           keytrack    = 0.0f;     // cents/key
        MidiNote        keycenter   = 60;
        float           veltrack    = 0.0f;
        float           random      = 0.0f;
    };

    // Effect slot definition
    struct EffectParams
    {
        EffectType  type    = EffectType::None;
        int         bus     = 0;
        int         dspOrder = 0;

        // Generic parameter map for effect-specific opcodes
        std::unordered_map<std::string, ModulatedFloat> params;
    };

    // Keyswitch definition
    struct KeyswitchParams
    {
        MidiNote    swLoKey     = 0;
        MidiNote    swHiKey     = 127;
        MidiNote    swLast      = 255;  // 255 = not set
        MidiNote    swDown      = 255;
        MidiNote    swUp        = 255;
        MidiNote    swPrevious  = 255;
        MidiNote    swDefault   = 255;
        enum class SwVel { Current, Previous } swVel = SwVel::Current;
    };

} // namespace sfz
