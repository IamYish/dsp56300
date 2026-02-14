#pragma once

#include "sfz/instrument.h"

#include <string>
#include <filesystem>
#include <functional>
#include <vector>

namespace sfz
{
    // Callback for parser diagnostics
    using ParserCallback = std::function<void(const std::string& message, int line, bool isError)>;

    // SFZ v2 parser.
    //
    // Parses an .sfz file (and any #include'd files) into an Instrument.
    // Handles:
    //   - Header hierarchy: <control>, <global>, <master>, <group>, <region>
    //   - <curve> and <effect> headers
    //   - #include "file" directives
    //   - #define $VAR value macros
    //   - Opcode inheritance (global -> master -> group -> region)
    //   - All SFZ v2 opcodes including flex EGs, flex LFOs, curves, effects
    class Parser
    {
    public:
        Parser();
        ~Parser();

        // Parse an SFZ file and return the fully resolved instrument.
        // Returns nullptr on failure.
        std::unique_ptr<Instrument> parse(const std::string& filePath);

        // Set diagnostic callback for warnings/errors during parsing
        void setCallback(ParserCallback cb) { m_callback = std::move(cb); }

    private:
        // Internal header types during parsing
        enum class HeaderType
        {
            None,
            Control,
            Global,
            Master,
            Group,
            Region,
            Curve,
            Effect,
            Sample,
            Midi,
        };

        // Pending opcode collection for current header
        struct OpcodeBlock
        {
            HeaderType                          type = HeaderType::None;
            std::vector<std::pair<std::string, std::string>>  opcodes;
        };

        // Parse the raw text of an SFZ file (or included file).
        // Recursion depth is tracked to prevent infinite #include loops.
        void parseText(const std::string& text, const std::filesystem::path& basePath,
                       int depth);

        // Process a single line of SFZ text.
        void processLine(const std::string& line, const std::filesystem::path& basePath,
                         int depth, int lineNumber);

        // Finalize the current opcode block (apply it to the instrument).
        void finalizeBlock();

        // Apply collected opcodes to a Region.
        void applyOpcodes(Region& region,
                          const std::vector<std::pair<std::string, std::string>>& opcodes);

        // Apply a single parsed opcode to a Region.
        void applyOpcode(Region& region, const ParsedOpcode& op);

        // Apply opcodes to a CurveDefinition.
        void applyCurveOpcodes(CurveDefinition& curve,
                               const std::vector<std::pair<std::string, std::string>>& opcodes);

        // Apply opcodes to an EffectParams.
        void applyEffectOpcodes(EffectParams& effect,
                                const std::vector<std::pair<std::string, std::string>>& opcodes);

        // Expand macros ($VAR) in a string.
        std::string expandMacros(const std::string& text) const;

        // Parse a note name (e.g., "c4", "C#5", "60") into a MIDI note number.
        static int parseNote(const std::string& str);

        // Parse "opcode=value opcode=value ..." text into the current block.
        void parseOpcodeText(const std::string& text, int lineNumber);

        // Diagnostic helper
        void warn(const std::string& msg, int line);
        void error(const std::string& msg, int line);

        // State
        std::unique_ptr<Instrument> m_instrument;
        ParserCallback              m_callback;

        OpcodeBlock     m_currentBlock;

        // Inheritance layers (accumulated opcodes at each level)
        std::vector<std::pair<std::string, std::string>>  m_globalOpcodes;
        std::vector<std::pair<std::string, std::string>>  m_masterOpcodes;
        std::vector<std::pair<std::string, std::string>>  m_groupOpcodes;

        int             m_groupIndex = 0;

        static constexpr int MaxIncludeDepth = 16;
    };

} // namespace sfz
