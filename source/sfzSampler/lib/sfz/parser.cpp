#include "parser.h"
#include "opcodes.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>

namespace sfz
{
    Parser::Parser() = default;
    Parser::~Parser() = default;

    std::unique_ptr<Instrument> Parser::parse(const std::string& filePath)
    {
        m_instrument = std::make_unique<Instrument>();
        m_instrument->filePath = filePath;
        m_instrument->initBuiltinCurves();

        m_currentBlock = {};
        m_globalOpcodes.clear();
        m_masterOpcodes.clear();
        m_groupOpcodes.clear();
        m_groupIndex = 0;

        const std::filesystem::path path(filePath);
        const auto basePath = path.parent_path();

        // Read the file
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            error("Failed to open file: " + filePath, 0);
            return nullptr;
        }

        std::stringstream ss;
        ss << file.rdbuf();
        const std::string text = ss.str();

        parseText(text, basePath, 0);

        // Finalize any pending block
        finalizeBlock();

        // Interpolate all custom curves
        for (auto& curve : m_instrument->curves)
            curve.interpolate();

        return std::move(m_instrument);
    }

    void Parser::parseText(const std::string& text, const std::filesystem::path& basePath,
                           int depth)
    {
        if (depth > MaxIncludeDepth)
        {
            error("Maximum #include depth exceeded", 0);
            return;
        }

        std::istringstream stream(text);
        std::string line;
        int lineNumber = 0;

        while (std::getline(stream, line))
        {
            ++lineNumber;

            // Strip comments (// to end of line)
            auto commentPos = line.find("//");
            if (commentPos != std::string::npos)
                line = line.substr(0, commentPos);

            // Trim whitespace
            auto start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                continue;
            line = line.substr(start);
            auto end = line.find_last_not_of(" \t\r\n");
            if (end != std::string::npos)
                line = line.substr(0, end + 1);

            if (line.empty())
                continue;

            processLine(line, basePath, depth, lineNumber);
        }
    }

    void Parser::processLine(const std::string& line, const std::filesystem::path& basePath,
                             int depth, int lineNumber)
    {
        // Expand macros first
        std::string expanded = expandMacros(line);

        // Handle #define
        if (expanded.substr(0, 7) == "#define")
        {
            auto rest = expanded.substr(7);
            auto varStart = rest.find('$');
            if (varStart != std::string::npos)
            {
                auto varEnd = rest.find_first_of(" \t", varStart);
                if (varEnd != std::string::npos)
                {
                    std::string varName = rest.substr(varStart, varEnd - varStart);
                    std::string varValue = rest.substr(varEnd);
                    // Trim value
                    auto vs = varValue.find_first_not_of(" \t");
                    if (vs != std::string::npos)
                        varValue = varValue.substr(vs);
                    m_instrument->macros[varName] = varValue;
                }
            }
            return;
        }

        // Handle #include
        if (expanded.substr(0, 8) == "#include")
        {
            auto quoteStart = expanded.find('"');
            auto quoteEnd = expanded.rfind('"');
            if (quoteStart != std::string::npos && quoteEnd > quoteStart)
            {
                std::string includePath = expanded.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                auto fullPath = basePath / includePath;

                std::ifstream incFile(fullPath);
                if (!incFile.is_open())
                {
                    error("Failed to open included file: " + fullPath.string(), lineNumber);
                    return;
                }

                std::stringstream ss;
                ss << incFile.rdbuf();
                parseText(ss.str(), fullPath.parent_path(), depth + 1);
            }
            return;
        }

        // Process the line: may contain headers and/or opcodes
        // Headers can appear inline: <region> sample=foo.wav lokey=36 hikey=38
        size_t pos = 0;
        while (pos < expanded.size())
        {
            // Look for a header tag
            auto headerStart = expanded.find('<', pos);
            if (headerStart != std::string::npos)
            {
                // Parse opcodes before the header
                if (headerStart > pos)
                {
                    std::string opcodeText = expanded.substr(pos, headerStart - pos);
                    parseOpcodeText(opcodeText, lineNumber);
                }

                auto headerEnd = expanded.find('>', headerStart);
                if (headerEnd == std::string::npos)
                {
                    warn("Unterminated header tag", lineNumber);
                    break;
                }

                std::string headerName = expanded.substr(headerStart + 1, headerEnd - headerStart - 1);
                // Trim
                auto hs = headerName.find_first_not_of(" \t");
                auto he = headerName.find_last_not_of(" \t");
                if (hs != std::string::npos)
                    headerName = headerName.substr(hs, he - hs + 1);

                // Finalize previous block
                finalizeBlock();

                // Start new block
                if (headerName == "control")
                    m_currentBlock.type = HeaderType::Control;
                else if (headerName == "global")
                    m_currentBlock.type = HeaderType::Global;
                else if (headerName == "master")
                    m_currentBlock.type = HeaderType::Master;
                else if (headerName == "group")
                {
                    m_currentBlock.type = HeaderType::Group;
                    ++m_groupIndex;
                }
                else if (headerName == "region")
                    m_currentBlock.type = HeaderType::Region;
                else if (headerName == "curve")
                    m_currentBlock.type = HeaderType::Curve;
                else if (headerName == "effect")
                    m_currentBlock.type = HeaderType::Effect;
                else if (headerName == "sample")
                    m_currentBlock.type = HeaderType::Sample;
                else if (headerName == "midi")
                    m_currentBlock.type = HeaderType::Midi;
                else
                    warn("Unknown header: <" + headerName + ">", lineNumber);

                pos = headerEnd + 1;
            }
            else
            {
                // Rest of line is opcodes
                std::string opcodeText = expanded.substr(pos);
                parseOpcodeText(opcodeText, lineNumber);
                break;
            }
        }
    }

    void Parser::finalizeBlock()
    {
        if (m_currentBlock.type == HeaderType::None || m_currentBlock.opcodes.empty())
        {
            m_currentBlock = {};
            return;
        }

        switch (m_currentBlock.type)
        {
        case HeaderType::Control:
            // Apply control opcodes to instrument settings
            for (const auto& [name, value] : m_currentBlock.opcodes)
            {
                if (name == "default_path")
                    m_instrument->defaultPath = value;
                else if (name == "note_offset")
                    m_instrument->noteOffset = std::stoi(value);
                else if (name == "octave_offset")
                    m_instrument->octaveOffset = std::stoi(value);
                else if (name.substr(0, 6) == "set_cc")
                {
                    int cc = std::stoi(name.substr(6));
                    if (cc >= 0 && cc < static_cast<int>(Config::MaxCCs))
                        m_instrument->defaultCC[cc] = std::stof(value);
                }
                else if (name.substr(0, 8) == "label_cc")
                {
                    int cc = std::stoi(name.substr(8));
                    m_instrument->ccLabels[cc] = value;
                }
            }
            break;

        case HeaderType::Global:
            m_globalOpcodes = m_currentBlock.opcodes;
            m_masterOpcodes.clear();
            m_groupOpcodes.clear();
            break;

        case HeaderType::Master:
            m_masterOpcodes = m_currentBlock.opcodes;
            m_groupOpcodes.clear();
            break;

        case HeaderType::Group:
            m_groupOpcodes = m_currentBlock.opcodes;
            break;

        case HeaderType::Region:
        {
            // Create a new region with full inheritance
            Region region;
            region.sourceGroupIndex = m_groupIndex;

            // Apply opcodes in inheritance order: global -> master -> group -> region
            applyOpcodes(region, m_globalOpcodes);
            applyOpcodes(region, m_masterOpcodes);
            applyOpcodes(region, m_groupOpcodes);
            applyOpcodes(region, m_currentBlock.opcodes);

            // Resolve sample path
            if (!region.sample.empty() && !m_instrument->defaultPath.empty())
            {
                // Prepend default_path if sample is not an absolute path
                if (region.sample[0] != '/' && region.sample[0] != '\\' &&
                    (region.sample.size() < 2 || region.sample[1] != ':'))
                {
                    region.sample = m_instrument->defaultPath + region.sample;
                }
            }

            m_instrument->regions.push_back(std::move(region));
            break;
        }

        case HeaderType::Curve:
        {
            CurveDefinition curve;
            applyCurveOpcodes(curve, m_currentBlock.opcodes);
            m_instrument->curves.push_back(std::move(curve));
            break;
        }

        case HeaderType::Effect:
        {
            EffectParams effect;
            applyEffectOpcodes(effect, m_currentBlock.opcodes);
            m_instrument->effects.push_back(std::move(effect));
            break;
        }

        default:
            break;
        }

        m_currentBlock = {};
    }

    void Parser::applyOpcodes(Region& region,
                              const std::vector<std::pair<std::string, std::string>>& opcodes)
    {
        for (const auto& [name, value] : opcodes)
        {
            auto parsed = parseOpcode(name, value);
            applyOpcode(region, parsed);
        }
    }

    void Parser::applyOpcode(Region& region, const ParsedOpcode& op)
    {
        const auto& name = op.baseName;
        const auto& val = op.value;

        // Helper lambdas
        auto toFloat = [&]() -> float { return std::stof(val); };
        auto toInt   = [&]() -> int   { return std::stoi(val); };

        // ---- Sample playback ----
        if (name == "sample")           { region.sample = val; return; }
        if (name == "offset")           { region.offset.base = toInt(); return; }
        if (name == "offset_random")    { region.offsetRandom = toInt(); return; }
        if (name == "end")              { region.end = toInt(); return; }
        if (name == "delay")            { region.delay.base = toFloat(); return; }
        if (name == "delay_random")     { region.delayRandom = toFloat(); return; }
        if (name == "loop_mode")        { if (auto lm = parseLoopMode(val)) region.loopMode = *lm; return; }
        if (name == "loop_start")       { region.loopStart = toInt(); return; }
        if (name == "loop_end")         { region.loopEnd = toInt(); return; }
        if (name == "loop_type")
        {
            if (val == "forward")       region.loopType = LoopType::Forward;
            else if (val == "backward") region.loopType = LoopType::Backward;
            else if (val == "alternate")region.loopType = LoopType::Alternate;
            return;
        }
        if (name == "loop_crossfade")   { region.loopCrossfade = toFloat(); return; }
        if (name == "loop_tune")        { region.loopTune = toFloat(); return; }
        if (name == "loop_count")       { region.loopCount = toInt(); return; }
        if (name == "direction")        { region.direction = (val != "reverse"); return; }
        if (name == "sample_quality")   { region.sampleQuality = toInt(); return; }

        // ---- Key mapping ----
        if (name == "lokey")            { region.keyRange.lo = static_cast<MidiNote>(parseNote(val)); return; }
        if (name == "hikey")            { region.keyRange.hi = static_cast<MidiNote>(parseNote(val)); return; }
        if (name == "key")
        {
            auto note = static_cast<MidiNote>(parseNote(val));
            region.keyRange.lo = note;
            region.keyRange.hi = note;
            region.pitchKeycenter = note;
            return;
        }
        if (name == "lovel")            { region.velRange.lo = static_cast<MidiVel>(toInt()); return; }
        if (name == "hivel")            { region.velRange.hi = static_cast<MidiVel>(toInt()); return; }
        if (name == "pitch_keycenter")  { region.pitchKeycenter = static_cast<MidiNote>(parseNote(val)); return; }

        // ---- MIDI conditions ----
        if (name == "lochan")           { region.loChan = static_cast<MidiChannel>(toInt() - 1); return; }
        if (name == "hichan")           { region.hiChan = static_cast<MidiChannel>(toInt() - 1); return; }
        if (name == "lobend")           { region.loBend = toInt(); return; }
        if (name == "hibend")           { region.hiBend = toInt(); return; }
        if (name == "lochanaft")        { region.loChanAft = toInt(); return; }
        if (name == "hichanaft")        { region.hiChanAft = toInt(); return; }
        if (name == "lopolyaft")        { region.loPolyAft = toInt(); return; }
        if (name == "hipolyaft")        { region.hiPolyAft = toInt(); return; }
        if (name == "loprog")           { region.loProg = toInt(); return; }
        if (name == "hiprog")           { region.hiProg = toInt(); return; }

        // ---- Internal conditions ----
        if (name == "lorand")           { region.loRand = toFloat(); return; }
        if (name == "hirand")           { region.hiRand = toFloat(); return; }
        if (name == "seq_length")       { region.seqLength = toInt(); return; }
        if (name == "seq_position")     { region.seqPosition = toInt(); return; }
        if (name == "lotimer")          { region.loTimer = toFloat(); return; }
        if (name == "hitimer")          { region.hiTimer = toFloat(); return; }
        if (name == "lobpm")            { region.loBpm = toFloat(); return; }
        if (name == "hibpm")            { region.hiBpm = toFloat(); return; }

        // ---- Trigger ----
        if (name == "trigger")          { if (auto t = parseTriggerType(val)) region.trigger = *t; return; }

        // ---- Voice lifecycle ----
        if (name == "group")            { region.voiceGroup = toInt(); return; }
        if (name == "off_by")           { region.offBy = toInt(); return; }
        if (name == "off_mode")         { region.offMode = (val == "normal") ? OffMode::Normal : OffMode::Fast; return; }
        if (name == "off_time")         { region.offTime = toFloat(); return; }
        if (name == "polyphony")        { region.polyphony = toInt(); return; }
        if (name == "note_polyphony")   { region.notePolyphony = toInt(); return; }
        if (name == "note_selfmask")    { region.noteSelfmask = (val == "on" || val == "1"); return; }
        if (name == "rt_dead")          { region.rtDead = (val == "on" || val == "1"); return; }
        if (name == "output")           { region.output = toInt(); return; }

        // ---- Amplifier ----
        if (name == "volume")           { region.volume.base = toFloat(); return; }
        if (name == "amplitude")        { region.amplitude.base = toFloat(); return; }
        if (name == "pan")              { region.pan.base = toFloat(); return; }
        if (name == "position")         { region.position.base = toFloat(); return; }
        if (name == "width")            { region.width.base = toFloat(); return; }
        if (name == "phase")            { region.phase = (val == "invert" || val == "1"); return; }
        if (name == "amp_keycenter")    { region.ampKeycenter = toFloat(); return; }
        if (name == "amp_keytrack")     { region.ampKeytrack = toFloat(); return; }
        if (name == "amp_veltrack")     { region.ampVeltrack = toFloat(); return; }
        if (name == "amp_random")       { region.ampRandom = toFloat(); return; }
        if (name == "gain_random")      { region.ampRandom = toFloat(); return; }

        // amp_velcurve_N
        if (name.substr(0, 14) == "amp_velcurve_")
        {
            int idx = std::stoi(name.substr(14));
            if (idx >= 0 && idx < 128)
            {
                region.ampVelcurve[idx] = toFloat();
                region.ampVelcurveUsed = true;
            }
            return;
        }

        // ---- Pitch ----
        if (name == "transpose")        { region.transpose = toInt(); return; }
        if (name == "tune" || name == "pitch")
        {
            region.tune.base = toFloat();
            return;
        }
        if (name == "pitch_random" || name == "tune_random")
        {
            region.pitchRandom = toFloat();
            return;
        }
        if (name == "pitch_keytrack")   { region.pitchKeytrack = toFloat(); return; }
        if (name == "pitch_veltrack")   { region.pitchVeltrack = toFloat(); return; }
        if (name == "bend_up")          { region.bendUp = toInt(); return; }
        if (name == "bend_down")        { region.bendDown = toInt(); return; }
        if (name == "bend_step")        { region.bendStep = toInt(); return; }
        if (name == "bend_smooth")      { region.bendSmooth = toFloat(); return; }

        // ---- Filters ----
        if (name == "fil_type")         { if (auto ft = parseFilterType(val)) region.filters[0].type = *ft; return; }
        if (name == "fil2_type")        { if (auto ft = parseFilterType(val)) region.filters[1].type = *ft; return; }
        if (name == "cutoff")           { region.filters[0].cutoff.base = toFloat(); return; }
        if (name == "cutoff2")          { region.filters[1].cutoff.base = toFloat(); return; }
        if (name == "resonance")        { region.filters[0].resonance.base = toFloat(); return; }
        if (name == "resonance2")       { region.filters[1].resonance.base = toFloat(); return; }
        if (name == "fil_keytrack")     { region.filters[0].keytrack = toFloat(); return; }
        if (name == "fil2_keytrack")    { region.filters[1].keytrack = toFloat(); return; }
        if (name == "fil_keycenter")    { region.filters[0].keycenter = static_cast<MidiNote>(toInt()); return; }
        if (name == "fil2_keycenter")   { region.filters[1].keycenter = static_cast<MidiNote>(toInt()); return; }
        if (name == "fil_veltrack")     { region.filters[0].veltrack = toFloat(); return; }
        if (name == "fil2_veltrack")    { region.filters[1].veltrack = toFloat(); return; }
        if (name == "fil_random" || name == "cutoff_random")
        {
            region.filters[0].random = toFloat();
            return;
        }
        if (name == "cutoff2_random")   { region.filters[1].random = toFloat(); return; }

        // ---- Fixed envelopes (ampeg_, fileg_, pitcheg_) ----
        if (op.category == OpcodeCategory::EnvelopeFixed)
        {
            EnvelopeParams* env = nullptr;
            std::string suffix;

            if (name.substr(0, 6) == "ampeg_")      { env = &region.ampEG; suffix = name.substr(6); }
            else if (name.substr(0, 6) == "fileg_")  { env = &region.filterEG; suffix = name.substr(6); }
            else if (name.substr(0, 8) == "pitcheg_"){ env = &region.pitchEG; suffix = name.substr(8); }

            if (env)
            {
                if (suffix == "delay")          env->delay.base = toFloat();
                else if (suffix == "start")     env->start.base = toFloat();
                else if (suffix == "attack")    env->attack.base = toFloat();
                else if (suffix == "hold")      env->hold.base = toFloat();
                else if (suffix == "decay")     env->decay.base = toFloat();
                else if (suffix == "sustain")   env->sustain.base = toFloat();
                else if (suffix == "release")   env->release.base = toFloat();
                else if (suffix == "depth")     env->depth = toFloat();
                else if (suffix == "vel2delay")   env->vel2delay = toFloat();
                else if (suffix == "vel2attack")  env->vel2attack = toFloat();
                else if (suffix == "vel2hold")    env->vel2hold = toFloat();
                else if (suffix == "vel2decay")   env->vel2decay = toFloat();
                else if (suffix == "vel2sustain") env->vel2sustain = toFloat();
                else if (suffix == "vel2release") env->vel2release = toFloat();
            }
            return;
        }

        // ---- Fixed LFOs (amplfo_, fillfo_, pitchlfo_) ----
        if (op.category == OpcodeCategory::LFOFixed)
        {
            FixedLFOParams* lfo = nullptr;
            std::string suffix;

            if (name.substr(0, 7) == "amplfo_")      { lfo = &region.ampLFO; suffix = name.substr(7); }
            else if (name.substr(0, 7) == "fillfo_")  { lfo = &region.filterLFO; suffix = name.substr(7); }
            else if (name.substr(0, 9) == "pitchlfo_"){ lfo = &region.pitchLFO; suffix = name.substr(9); }

            if (lfo)
            {
                if (suffix == "delay")      lfo->delay.base = toFloat();
                else if (suffix == "depth") lfo->depth.base = toFloat();
                else if (suffix == "freq")  lfo->freq.base = toFloat();
                else if (suffix == "fade")  lfo->fade.base = toFloat();
            }
            return;
        }

        // ---- EQ bands ----
        if (op.category == OpcodeCategory::EQ && op.index1 >= 1 && op.index1 <= 3)
        {
            auto& band = region.eq[op.index1 - 1];
            const auto& base = op.baseName;

            if (base == "eqN_freq")        band.freq.base = toFloat();
            else if (base == "eqN_bw")     band.bw.base = toFloat();
            else if (base == "eqN_gain")   band.gain.base = toFloat();
            else if (base == "eqN_type")   { if (auto t = parseEqType(val)) band.type = *t; }
            else if (base == "eqN_vel2freq") band.vel2freq = toFloat();
            else if (base == "eqN_vel2gain") band.vel2gain = toFloat();
            return;
        }

        // ---- Effect sends ----
        if (name == "effect1")          { region.effect1 = toFloat(); return; }
        if (name == "effect2")          { region.effect2 = toFloat(); return; }
        if (name == "effect3")          { region.effect3 = toFloat(); return; }
        if (name == "effect4")          { region.effect4 = toFloat(); return; }

        // ---- Keyswitches ----
        if (name == "sw_lokey")         { region.keyswitch.swLoKey = static_cast<MidiNote>(parseNote(val)); return; }
        if (name == "sw_hikey")         { region.keyswitch.swHiKey = static_cast<MidiNote>(parseNote(val)); return; }
        if (name == "sw_last")          { region.keyswitch.swLast = static_cast<MidiNote>(parseNote(val)); return; }
        if (name == "sw_down")          { region.keyswitch.swDown = static_cast<MidiNote>(parseNote(val)); return; }
        if (name == "sw_up")            { region.keyswitch.swUp = static_cast<MidiNote>(parseNote(val)); return; }
        if (name == "sw_previous")      { region.keyswitch.swPrevious = static_cast<MidiNote>(parseNote(val)); return; }
        if (name == "sw_default")       { region.keyswitch.swDefault = static_cast<MidiNote>(parseNote(val)); return; }
        if (name == "sw_vel")
        {
            region.keyswitch.swVel = (val == "previous")
                ? KeyswitchParams::SwVel::Previous
                : KeyswitchParams::SwVel::Current;
            return;
        }

        // ---- Wavetable ----
        if (name == "oscillator")           { region.oscillator = (val == "on" || val == "1"); return; }
        if (name == "oscillator_mode")      { region.oscillatorMode = toInt(); return; }
        if (name == "oscillator_multi")     { region.oscillatorMulti = toInt(); return; }
        if (name == "oscillator_phase")     { region.oscillatorPhase = toFloat(); return; }
        if (name == "oscillator_quality")   { region.oscillatorQuality = toInt(); return; }
        if (name == "oscillator_table_size"){ region.oscillatorTableSize = toInt(); return; }

        // Unrecognized opcodes are silently ignored for forward compatibility.
    }

    void Parser::applyCurveOpcodes(CurveDefinition& curve,
                                   const std::vector<std::pair<std::string, std::string>>& opcodes)
    {
        for (const auto& [name, value] : opcodes)
        {
            if (name == "curve_index")
            {
                curve.setIndex(std::stoi(value));
            }
            else if (name.size() >= 2 && name[0] == 'v')
            {
                // vNNN = value
                int point = std::stoi(name.substr(1));
                curve.setPoint(point, std::stof(value));
            }
        }
    }

    void Parser::applyEffectOpcodes(EffectParams& effect,
                                    const std::vector<std::pair<std::string, std::string>>& opcodes)
    {
        for (const auto& [name, value] : opcodes)
        {
            if (name == "type")
            {
                if (auto et = parseEffectType(value))
                    effect.type = *et;
            }
            else if (name == "bus")
            {
                // bus=main, bus=aux1, bus=fx1, etc.
                if (value == "main") effect.bus = 0;
                else
                {
                    // Try to parse numeric part
                    std::string numStr;
                    for (char c : value)
                        if (std::isdigit(c)) numStr += c;
                    if (!numStr.empty())
                        effect.bus = std::stoi(numStr);
                }
            }
            else if (name == "dsp_order")
            {
                effect.dspOrder = std::stoi(value);
            }
            else
            {
                // Store as generic parameter
                effect.params[name] = ModulatedFloat(std::stof(value));
            }
        }
    }

    std::string Parser::expandMacros(const std::string& text) const
    {
        std::string result = text;
        for (const auto& [name, value] : m_instrument->macros)
        {
            size_t pos = 0;
            while ((pos = result.find(name, pos)) != std::string::npos)
            {
                result.replace(pos, name.size(), value);
                pos += value.size();
            }
        }
        return result;
    }

    int Parser::parseNote(const std::string& str)
    {
        // Try numeric first
        if (!str.empty() && (std::isdigit(str[0]) || str[0] == '-'))
        {
            return std::stoi(str);
        }

        // Parse note name: C-1 through G9
        // Formats: c4, C4, C#4, Cb4, c#-1, etc.
        if (str.empty()) return 60;

        int note = 0;
        size_t pos = 0;

        char noteLetter = static_cast<char>(std::toupper(str[0]));
        switch (noteLetter)
        {
            case 'C': note = 0; break;
            case 'D': note = 2; break;
            case 'E': note = 4; break;
            case 'F': note = 5; break;
            case 'G': note = 7; break;
            case 'A': note = 9; break;
            case 'B': note = 11; break;
            default: return 60;
        }
        pos = 1;

        // Check for sharp/flat
        if (pos < str.size())
        {
            if (str[pos] == '#' || str[pos] == 's')         { note += 1; ++pos; }
            else if (str[pos] == 'b' && pos + 1 < str.size() &&
                     (std::isdigit(str[pos + 1]) || str[pos + 1] == '-'))
            {
                note -= 1; ++pos;
            }
        }

        // Parse octave
        if (pos < str.size())
        {
            int octave = std::stoi(str.substr(pos));
            note += (octave + 1) * 12;
        }

        return std::clamp(note, 0, 127);
    }

    void Parser::warn(const std::string& msg, int line)
    {
        if (m_callback) m_callback("Warning: " + msg, line, false);
    }

    void Parser::error(const std::string& msg, int line)
    {
        if (m_callback) m_callback("Error: " + msg, line, true);
    }

    void Parser::parseOpcodeText(const std::string& text, int /*lineNumber*/)
    {
        // Parse "opcode=value opcode=value ..." sequences.
        // Values may contain spaces if they are file paths (sample=My File.wav),
        // so we split on the next '=' and look backwards for the key boundary.
        std::string remaining = text;

        // Trim leading whitespace
        auto trimStart = remaining.find_first_not_of(" \t");
        if (trimStart == std::string::npos)
            return;
        remaining = remaining.substr(trimStart);

        while (!remaining.empty())
        {
            // Find the first '='
            auto eqPos = remaining.find('=');
            if (eqPos == std::string::npos)
                break; // No more opcodes

            std::string key = remaining.substr(0, eqPos);
            // Trim key
            auto ks = key.find_last_of(" \t");
            if (ks != std::string::npos)
                key = key.substr(ks + 1);

            remaining = remaining.substr(eqPos + 1);

            // Find the value: everything until the next opcode (next word followed by '=')
            std::string value;
            auto nextEq = remaining.find('=');
            if (nextEq != std::string::npos)
            {
                // Find the start of the next opcode key (last word before '=')
                auto boundary = remaining.rfind(' ', nextEq);
                auto boundary2 = remaining.rfind('\t', nextEq);
                if (boundary2 != std::string::npos &&
                    (boundary == std::string::npos || boundary2 > boundary))
                    boundary = boundary2;

                if (boundary != std::string::npos)
                {
                    value = remaining.substr(0, boundary);
                    remaining = remaining.substr(boundary + 1);
                }
                else
                {
                    value = remaining;
                    remaining.clear();
                }
            }
            else
            {
                value = remaining;
                remaining.clear();
            }

            // Trim value
            auto vs = value.find_first_not_of(" \t");
            auto ve = value.find_last_not_of(" \t\r\n");
            if (vs != std::string::npos && ve != std::string::npos)
                value = value.substr(vs, ve - vs + 1);
            else
                value.clear();

            // Convert key to lowercase for case-insensitive matching
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (!key.empty() && !value.empty())
                m_currentBlock.opcodes.emplace_back(key, value);
        }
    }

} // namespace sfz
