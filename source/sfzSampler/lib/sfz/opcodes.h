#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>

namespace sfz
{
    // Opcode category classification
    enum class OpcodeCategory
    {
        SamplePlayback,
        KeyMapping,
        MidiCondition,
        InternalCondition,
        Trigger,
        VoiceLifecycle,
        Amplifier,
        Pitch,
        Filter,
        EQ,
        EnvelopeFixed,      // ampeg_, fileg_, pitcheg_
        EnvelopeFlex,       // egN_
        LFOFixed,           // amplfo_, fillfo_, pitchlfo_
        LFOFlex,            // lfoN_
        Curve,
        Effect,
        Instrument,
        Wavetable,
        Unknown,
    };

    // Parsed opcode: name decomposed into base + optional indices
    struct ParsedOpcode
    {
        std::string     fullName;       // original text, e.g. "lfo2_freq_oncc74"
        std::string     baseName;       // canonical base, e.g. "lfoN_freq"
        OpcodeCategory  category = OpcodeCategory::Unknown;

        // Numeric indices extracted from the name
        int             index1 = -1;    // e.g. N in lfoN, egN, eqN, filN
        int             index2 = -1;    // e.g. X in egN_timeX, lfoN_stepX
        int             ccIndex = -1;   // e.g. N in _onccN, _curveccN

        // Modifier type (if any)
        enum class ModType { None, OnCC, CurveCC, SmoothCC, StepCC };
        ModType         modType = ModType::None;

        std::string     value;          // raw string value
    };

    // Parse an opcode string into its components.
    // Handles patterns like: lfo2_freq_oncc74, eg3_time2_oncc1, eq1_freq, etc.
    ParsedOpcode parseOpcode(const std::string& name, const std::string& value);

    // Convert string to filter type
    std::optional<FilterType> parseFilterType(const std::string& str);

    // Convert string to loop mode
    std::optional<LoopMode> parseLoopMode(const std::string& str);

    // Convert string to trigger type
    std::optional<TriggerType> parseTriggerType(const std::string& str);

    // Convert string to LFO wave type
    std::optional<LfoWave> parseLfoWave(const std::string& str);

    // Convert string to effect type
    std::optional<EffectType> parseEffectType(const std::string& str);

    // Convert string to EQ type
    std::optional<EqType> parseEqType(const std::string& str);

} // namespace sfz
