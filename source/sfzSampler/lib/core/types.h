#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <optional>
#include <variant>
#include <functional>

namespace sfz
{
    // MIDI types
    using MidiChannel = uint8_t;    // 0-15
    using MidiNote    = uint8_t;    // 0-127
    using MidiCC      = uint16_t;   // 0-137 (extended SFZ v2 range)
    using MidiVel     = uint8_t;    // 0-127

    // Audio types
    using SampleRate  = uint32_t;
    using FrameCount  = uint32_t;

    // MIDI message (compact, lock-free friendly)
    struct MidiMessage
    {
        enum class Type : uint8_t
        {
            NoteOn,
            NoteOff,
            ControlChange,
            PitchBend,
            ChannelPressure,
            PolyPressure,
            ProgramChange,
        };

        Type        type;
        MidiChannel channel;    // 0-15
        uint8_t     data1;      // note / cc number / program
        uint8_t     data2;      // velocity / cc value / pressure
        int16_t     pitchBend;  // -8192 to 8191 (for PitchBend type)
        uint32_t    timestamp;  // sample offset within current buffer
    };

    // SFZ extended CC identifiers (beyond standard 0-127)
    enum ExtendedCC : uint16_t
    {
        CC_PitchBend       = 128,
        CC_ChannelAT       = 129,
        CC_PolyAT          = 130,
        CC_NoteOnVel       = 131,
        CC_NoteOffVel      = 132,
        CC_KeyNumber        = 133,
        CC_NoteGate        = 134,
        CC_UniRandom       = 135,
        CC_BiRandom        = 136,
        CC_Alternate       = 137,
    };

    // Trigger types for regions
    enum class TriggerType
    {
        Attack,
        Release,
        ReleaseKey,
        First,
        Legato,
    };

    // Loop modes
    enum class LoopMode
    {
        NoLoop,
        OneShot,
        LoopContinuous,
        LoopSustain,
    };

    // Loop direction types
    enum class LoopType
    {
        Forward,
        Backward,
        Alternate,
    };

    // Filter types (SFZ v2 full set)
    enum class FilterType
    {
        None,
        LPF_1P,     // 1-pole low pass (6 dB/oct)
        HPF_1P,     // 1-pole high pass
        BPF_1P,     // 1-pole band pass
        BRF_1P,     // 1-pole band reject
        APF_1P,     // 1-pole all pass
        LPF_2P,     // 2-pole low pass (12 dB/oct) - default
        HPF_2P,     // 2-pole high pass
        BPF_2P,     // 2-pole band pass
        BRF_2P,     // 2-pole band reject
        PKF_2P,     // 2-pole peaking EQ
        LPF_4P,     // 4-pole low pass (24 dB/oct)
        HPF_4P,     // 4-pole high pass
        LPF_6P,     // 6-pole low pass (36 dB/oct)
        HPF_6P,     // 6-pole high pass
        Comb,       // Comb filter
        Pink,       // Pink noise filter
        LSH,        // Low shelf (ARIA extension)
        HSH,        // High shelf (ARIA extension)
        PEQ,        // Parametric EQ (ARIA extension)
    };

    // EQ band types
    enum class EqType
    {
        Peak,
        LowShelf,
        HighShelf,
    };

    // Crossfade curve types
    enum class CrossfadeCurve
    {
        Gain,
        Power,
    };

    // Off mode (voice stealing)
    enum class OffMode
    {
        Fast,
        Normal,
    };

    // LFO waveform shapes
    enum class LfoWave
    {
        Triangle  = 0,
        Sine      = 1,
        Pulse75   = 2,
        Square    = 3,
        Pulse25   = 4,
        Pulse12   = 5,
        SawUp     = 6,
        SawDown   = 7,
    };

    // Effect types
    enum class EffectType
    {
        None,
        Reverb,
        Delay,
        EQ,
        Distortion,
        Compressor,
        Gate,
        Phaser,
        AutoPan,
        Static,
        Strings,
        TDFIR,      // Convolution
    };

    // CC modulation with optional curve/smooth/step
    struct CCModulation
    {
        MidiCC  cc      = 0;
        float   depth   = 0.0f;
        int     curveIndex = -1;    // -1 = use default
        float   smooth  = 0.0f;    // smoothing in ms
        float   step    = 0.0f;    // quantization step
    };

    // Modulation-capable float value: base value + CC modulations
    struct ModulatedFloat
    {
        float                       base = 0.0f;
        std::vector<CCModulation>   mods;

        ModulatedFloat() = default;
        explicit ModulatedFloat(float v) : base(v) {}
    };

    // Modulation-capable int value
    struct ModulatedInt
    {
        int                         base = 0;
        std::vector<CCModulation>   mods;

        ModulatedInt() = default;
        explicit ModulatedInt(int v) : base(v) {}
    };

} // namespace sfz
