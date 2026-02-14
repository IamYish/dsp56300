#include "opcodes.h"

#include <regex>
#include <charconv>
#include <algorithm>

namespace sfz
{
    namespace
    {
        // Extract trailing integer from string, modifying the string to remove it.
        int extractTrailingInt(std::string& s)
        {
            size_t pos = s.size();
            while (pos > 0 && std::isdigit(s[pos - 1]))
                --pos;

            if (pos == s.size())
                return -1;

            int val = 0;
            std::from_chars(s.data() + pos, s.data() + s.size(), val);
            s.resize(pos);
            return val;
        }

        // Check if string ends with a suffix, and extract the CC index.
        bool extractCCSuffix(std::string& base, const std::string& suffix, int& ccIndex)
        {
            if (base.size() > suffix.size())
            {
                const size_t suffixStart = base.size() - suffix.size();
                bool isSuffix = true;

                // Check character by character, with digits as wildcards
                size_t digitStart = std::string::npos;
                for (size_t i = 0; i < suffix.size(); ++i)
                {
                    if (base[suffixStart + i] != suffix[i])
                    {
                        isSuffix = false;
                        break;
                    }
                }

                if (!isSuffix)
                {
                    // Try matching with digits before the suffix keyword
                    // e.g., _oncc74 -> suffix="_oncc", digits="74"
                    // The digits come AFTER the suffix in the full name
                }
            }

            // Simpler approach: look for the suffix pattern followed by digits
            auto pos = base.find(suffix);
            if (pos != std::string::npos && pos + suffix.size() < base.size())
            {
                std::string numStr = base.substr(pos + suffix.size());
                if (!numStr.empty() && std::all_of(numStr.begin(), numStr.end(), ::isdigit))
                {
                    ccIndex = std::stoi(numStr);
                    base = base.substr(0, pos);
                    return true;
                }
            }
            return false;
        }
    }

    ParsedOpcode parseOpcode(const std::string& name, const std::string& value)
    {
        ParsedOpcode result;
        result.fullName = name;
        result.value = value;

        std::string working = name;

        // 1. Extract CC modulation suffix (_onccN, _curveccN, _smoothccN, _stepccN)
        if (extractCCSuffix(working, "_oncc", result.ccIndex))
            result.modType = ParsedOpcode::ModType::OnCC;
        else if (extractCCSuffix(working, "_curvecc", result.ccIndex))
            result.modType = ParsedOpcode::ModType::CurveCC;
        else if (extractCCSuffix(working, "_smoothcc", result.ccIndex))
            result.modType = ParsedOpcode::ModType::SmoothCC;
        else if (extractCCSuffix(working, "_stepcc", result.ccIndex))
            result.modType = ParsedOpcode::ModType::StepCC;
        // Also handle legacy _ccN suffix (SFZ v1)
        else if (extractCCSuffix(working, "_cc", result.ccIndex))
            result.modType = ParsedOpcode::ModType::OnCC;

        // 2. Categorize and extract indices based on prefix
        // Flex EG: egN_* pattern
        if (working.substr(0, 2) == "eg" && working.size() > 2 && std::isdigit(working[2]))
        {
            result.category = OpcodeCategory::EnvelopeFlex;
            // Extract N from egN
            size_t underscorePos = working.find('_');
            if (underscorePos != std::string::npos)
            {
                result.index1 = std::stoi(working.substr(2, underscorePos - 2));
                std::string remainder = working.substr(underscorePos + 1);

                // Check for second index (e.g., time2, level3)
                // Patterns: timeX, levelX, shapeX, curveX
                for (const auto& prefix : {"time", "level", "shape", "curve"})
                {
                    std::string p(prefix);
                    if (remainder.substr(0, p.size()) == p)
                    {
                        std::string numPart = remainder.substr(p.size());
                        if (!numPart.empty() && std::all_of(numPart.begin(), numPart.end(), ::isdigit))
                        {
                            result.index2 = std::stoi(numPart);
                            remainder = p;
                        }
                        break;
                    }
                }
                result.baseName = "egN_" + remainder;
            }
            else
            {
                result.baseName = working;
            }
        }
        // Flex LFO: lfoN_* pattern
        else if (working.substr(0, 3) == "lfo" && working.size() > 3 && std::isdigit(working[3]))
        {
            result.category = OpcodeCategory::LFOFlex;
            size_t underscorePos = working.find('_');
            if (underscorePos != std::string::npos)
            {
                result.index1 = std::stoi(working.substr(3, underscorePos - 3));
                std::string remainder = working.substr(underscorePos + 1);

                // Check for step index (stepX)
                if (remainder.substr(0, 4) == "step" && remainder.size() > 4)
                {
                    std::string numPart = remainder.substr(4);
                    if (std::all_of(numPart.begin(), numPart.end(), ::isdigit))
                    {
                        result.index2 = std::stoi(numPart);
                        remainder = "step";
                    }
                }
                result.baseName = "lfoN_" + remainder;
            }
            else
            {
                result.baseName = working;
            }
        }
        // EQ: eqN_* pattern
        else if (working.substr(0, 2) == "eq" && working.size() > 2 && std::isdigit(working[2]))
        {
            result.category = OpcodeCategory::EQ;
            result.index1 = working[2] - '0';  // eq1, eq2, eq3
            if (working.size() > 3 && working[3] == '_')
                result.baseName = "eqN" + working.substr(3);
            else
                result.baseName = working;
        }
        // Fixed envelope: ampeg_, fileg_, pitcheg_
        else if (working.substr(0, 6) == "ampeg_" ||
                 working.substr(0, 6) == "fileg_" ||
                 working.substr(0, 8) == "pitcheg_")
        {
            result.category = OpcodeCategory::EnvelopeFixed;
            result.baseName = working;
        }
        // Fixed LFO: amplfo_, fillfo_, pitchlfo_
        else if (working.substr(0, 6) == "amplfo_" ||
                 working.substr(0, 6) == "fillfo_" ||
                 working.substr(0, 8) == "pitchlfo_")
        {
            result.category = OpcodeCategory::LFOFixed;
            result.baseName = working;
        }
        // Filter: fil_type, fil2_type, cutoff, cutoff2, resonance, resonance2
        else if (working.substr(0, 3) == "fil" || working.substr(0, 6) == "cutoff" ||
                 working.substr(0, 9) == "resonance")
        {
            result.category = OpcodeCategory::Filter;
            result.baseName = working;
        }
        // Sample playback
        else if (working == "sample" || working == "offset" || working == "end" ||
                 working.substr(0, 4) == "loop" || working == "direction" ||
                 working.substr(0, 5) == "delay" || working == "sample_quality")
        {
            result.category = OpcodeCategory::SamplePlayback;
            result.baseName = working;
        }
        // Key mapping
        else if (working == "lokey" || working == "hikey" || working == "key" ||
                 working == "lovel" || working == "hivel" || working == "pitch_keycenter")
        {
            result.category = OpcodeCategory::KeyMapping;
            result.baseName = working;
        }
        // Amplifier
        else if (working == "volume" || working == "amplitude" || working == "pan" ||
                 working == "position" || working == "width" || working == "phase" ||
                 working.substr(0, 3) == "amp" || working.substr(0, 4) == "xfin" ||
                 working.substr(0, 5) == "xfout" || working.substr(0, 5) == "xf_cc" ||
                 working.substr(0, 5) == "xf_ke" || working.substr(0, 5) == "xf_ve" ||
                 working == "gain_random")
        {
            result.category = OpcodeCategory::Amplifier;
            result.baseName = working;
        }
        // Pitch
        else if (working == "transpose" || working == "tune" || working == "pitch" ||
                 working == "pitch_random" || working == "tune_random" ||
                 working == "pitch_keytrack" || working == "pitch_veltrack" ||
                 working.substr(0, 4) == "bend")
        {
            result.category = OpcodeCategory::Pitch;
            result.baseName = working;
        }
        // Trigger
        else if (working == "trigger" || working.substr(0, 3) == "on_" ||
                 working.substr(0, 6) == "start_" || working.substr(0, 5) == "stop_")
        {
            result.category = OpcodeCategory::Trigger;
            result.baseName = working;
        }
        // Voice lifecycle
        else if (working == "group" || working == "off_by" || working == "off_mode" ||
                 working == "off_time" || working == "polyphony" ||
                 working == "note_polyphony" || working == "note_selfmask" ||
                 working == "output" || working == "rt_dead")
        {
            result.category = OpcodeCategory::VoiceLifecycle;
            result.baseName = working;
        }
        // MIDI conditions
        else if (working.substr(0, 2) == "lo" || working.substr(0, 2) == "hi" ||
                 working.substr(0, 2) == "sw")
        {
            result.category = OpcodeCategory::MidiCondition;
            result.baseName = working;
        }
        // Internal conditions
        else if (working == "seq_length" || working == "seq_position" ||
                 working.substr(0, 5) == "lorand" || working.substr(0, 5) == "hirand" ||
                 working.substr(0, 5) == "lobpm" || working.substr(0, 5) == "hibpm" ||
                 working.substr(0, 7) == "lotimer" || working.substr(0, 7) == "hitimer")
        {
            result.category = OpcodeCategory::InternalCondition;
            result.baseName = working;
        }
        // Effect
        else if (working == "type" || working == "bus" || working == "dsp_order" ||
                 working.substr(0, 6) == "effect" || working.substr(0, 6) == "reverb" ||
                 working.substr(0, 5) == "delay" || working.substr(0, 5) == "disto" ||
                 working.substr(0, 4) == "comp" || working.substr(0, 4) == "gate" ||
                 working.substr(0, 6) == "phaser" || working.substr(0, 4) == "apan" ||
                 working.substr(0, 6) == "static" || working.substr(0, 5) == "tdfir")
        {
            result.category = OpcodeCategory::Effect;
            result.baseName = working;
        }
        // Instrument-level
        else if (working == "default_path" || working == "note_offset" ||
                 working == "octave_offset")
        {
            result.category = OpcodeCategory::Instrument;
            result.baseName = working;
        }
        // Wavetable
        else if (working.substr(0, 10) == "oscillator")
        {
            result.category = OpcodeCategory::Wavetable;
            result.baseName = working;
        }
        else
        {
            result.category = OpcodeCategory::Unknown;
            result.baseName = working;
        }

        return result;
    }

    std::optional<FilterType> parseFilterType(const std::string& str)
    {
        static const std::unordered_map<std::string, FilterType> map = {
            {"lpf_1p", FilterType::LPF_1P}, {"hpf_1p", FilterType::HPF_1P},
            {"bpf_1p", FilterType::BPF_1P}, {"brf_1p", FilterType::BRF_1P},
            {"apf_1p", FilterType::APF_1P},
            {"lpf_2p", FilterType::LPF_2P}, {"hpf_2p", FilterType::HPF_2P},
            {"bpf_2p", FilterType::BPF_2P}, {"brf_2p", FilterType::BRF_2P},
            {"pkf_2p", FilterType::PKF_2P},
            {"lpf_4p", FilterType::LPF_4P}, {"hpf_4p", FilterType::HPF_4P},
            {"lpf_6p", FilterType::LPF_6P}, {"hpf_6p", FilterType::HPF_6P},
            {"comb", FilterType::Comb}, {"pink", FilterType::Pink},
            {"lsh", FilterType::LSH}, {"hsh", FilterType::HSH}, {"peq", FilterType::PEQ},
        };
        auto it = map.find(str);
        return it != map.end() ? std::optional{it->second} : std::nullopt;
    }

    std::optional<LoopMode> parseLoopMode(const std::string& str)
    {
        static const std::unordered_map<std::string, LoopMode> map = {
            {"no_loop", LoopMode::NoLoop}, {"one_shot", LoopMode::OneShot},
            {"loop_continuous", LoopMode::LoopContinuous},
            {"loop_sustain", LoopMode::LoopSustain},
        };
        auto it = map.find(str);
        return it != map.end() ? std::optional{it->second} : std::nullopt;
    }

    std::optional<TriggerType> parseTriggerType(const std::string& str)
    {
        static const std::unordered_map<std::string, TriggerType> map = {
            {"attack", TriggerType::Attack}, {"release", TriggerType::Release},
            {"release_key", TriggerType::ReleaseKey},
            {"first", TriggerType::First}, {"legato", TriggerType::Legato},
        };
        auto it = map.find(str);
        return it != map.end() ? std::optional{it->second} : std::nullopt;
    }

    std::optional<LfoWave> parseLfoWave(const std::string& str)
    {
        // Can be specified as integer 0-7 or name
        if (!str.empty() && std::isdigit(str[0]))
        {
            const int val = std::stoi(str);
            if (val >= 0 && val <= 7)
                return static_cast<LfoWave>(val);
            return std::nullopt;
        }

        static const std::unordered_map<std::string, LfoWave> map = {
            {"triangle", LfoWave::Triangle}, {"sine", LfoWave::Sine},
            {"square", LfoWave::Square},
            {"saw_up", LfoWave::SawUp}, {"saw_down", LfoWave::SawDown},
        };
        auto it = map.find(str);
        return it != map.end() ? std::optional{it->second} : std::nullopt;
    }

    std::optional<EffectType> parseEffectType(const std::string& str)
    {
        static const std::unordered_map<std::string, EffectType> map = {
            {"reverb", EffectType::Reverb}, {"delay", EffectType::Delay},
            {"eq", EffectType::EQ}, {"disto", EffectType::Distortion},
            {"comp", EffectType::Compressor}, {"gate", EffectType::Gate},
            {"phaser", EffectType::Phaser}, {"apan", EffectType::AutoPan},
            {"static", EffectType::Static}, {"strings", EffectType::Strings},
            {"tdfir", EffectType::TDFIR},
        };
        auto it = map.find(str);
        return it != map.end() ? std::optional{it->second} : std::nullopt;
    }

    std::optional<EqType> parseEqType(const std::string& str)
    {
        static const std::unordered_map<std::string, EqType> map = {
            {"peak", EqType::Peak}, {"lshelf", EqType::LowShelf},
            {"hshelf", EqType::HighShelf},
        };
        auto it = map.find(str);
        return it != map.end() ? std::optional{it->second} : std::nullopt;
    }

} // namespace sfz
