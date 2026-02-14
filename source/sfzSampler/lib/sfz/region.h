#pragma once

#include "sfz/types.h"
#include "core/config.h"

#include <string>
#include <vector>
#include <array>
#include <optional>

namespace sfz
{
    // A fully-resolved SFZ region.
    // After parsing and opcode inheritance (global -> master -> group -> region),
    // every region is a self-contained description of how to play a sample.
    struct Region
    {
        // ---- Sample playback ------------------------------------------------
        std::string     sample;                         // path to audio file
        ModulatedInt    offset      {0};                // sample start offset
        int             offsetRandom = 0;
        int             end         = -1;               // sample end (-1 = full)
        ModulatedFloat  delay       {0.0f};             // delay before playback (s)
        float           delayRandom = 0.0f;
        LoopMode        loopMode    = LoopMode::NoLoop;
        int             loopStart   = -1;
        int             loopEnd     = -1;
        LoopType        loopType    = LoopType::Forward;
        float           loopCrossfade = 0.0f;
        float           loopTune    = 0.0f;
        int             loopCount   = 0;                // 0 = infinite
        bool            direction   = true;             // true = forward
        int             sampleQuality = -1;             // interpolation quality

        // ---- Key / velocity mapping -----------------------------------------
        KeyRange        keyRange    {0, 127};
        VelRange        velRange    {0, 127};
        MidiNote        pitchKeycenter = 60;

        // ---- MIDI conditions ------------------------------------------------
        MidiChannel     loChan  = 0;
        MidiChannel     hiChan  = 15;
        int             loBend  = -8192;
        int             hiBend  = 8192;
        int             loChanAft = 0;
        int             hiChanAft = 127;
        int             loPolyAft = 0;
        int             hiPolyAft = 127;
        int             loProg  = -1;
        int             hiProg  = -1;

        // CC range conditions
        std::vector<CCRange>    ccConditions;

        // ---- Keyswitches ----------------------------------------------------
        KeyswitchParams keyswitch;

        // ---- Internal conditions --------------------------------------------
        float           loRand      = 0.0f;
        float           hiRand      = 1.0f;
        int             seqLength   = 1;
        int             seqPosition = 1;
        float           loTimer     = 0.0f;
        float           hiTimer     = 0.0f;
        float           loBpm       = 0.0f;
        float           hiBpm       = 500.0f;

        // ---- Trigger --------------------------------------------------------
        TriggerType     trigger     = TriggerType::Attack;

        // ---- Voice lifecycle ------------------------------------------------
        int             voiceGroup      = 0;            // group (opcode)
        int             offBy           = 0;
        OffMode         offMode         = OffMode::Fast;
        float           offTime         = 0.006f;       // seconds
        int             polyphony       = 0;            // 0 = unlimited
        int             notePolyphony   = 0;
        bool            noteSelfmask    = true;
        bool            rtDead          = false;
        int             output          = 0;

        // ---- Amplifier ------------------------------------------------------
        ModulatedFloat  volume      {0.0f};             // dB
        ModulatedFloat  amplitude   {100.0f};           // 0-100%
        ModulatedFloat  pan         {0.0f};             // -100 to 100
        ModulatedFloat  position    {0.0f};             // -100 to 100
        ModulatedFloat  width       {100.0f};           // -100 to 100
        bool            phase       = false;            // phase inversion
        float           ampKeycenter = 60.0f;
        float           ampKeytrack  = 0.0f;
        float           ampVeltrack  = 100.0f;
        float           ampRandom    = 0.0f;
        float           ampVelcurve[128] = {};          // custom velocity curve (0 = use default)
        bool            ampVelcurveUsed  = false;

        // Crossfade ranges
        KeyRange        xfinKey     {0, 0};
        KeyRange        xfoutKey    {127, 127};
        VelRange        xfinVel     {0, 0};
        VelRange        xfoutVel    {127, 127};
        std::vector<CCRange>    xfinCC;
        std::vector<CCRange>    xfoutCC;
        CrossfadeCurve  xfKeyCurve  = CrossfadeCurve::Power;
        CrossfadeCurve  xfVelCurve  = CrossfadeCurve::Power;
        CrossfadeCurve  xfCCCurve   = CrossfadeCurve::Power;

        // ---- Pitch ----------------------------------------------------------
        int             transpose       = 0;            // semitones
        ModulatedFloat  tune            {0.0f};         // cents
        float           pitchRandom     = 0.0f;
        float           pitchKeytrack   = 100.0f;       // cents/key
        float           pitchVeltrack   = 0.0f;
        int             bendUp          = 200;          // cents
        int             bendDown        = -200;
        int             bendStep        = 1;
        float           bendSmooth      = 0.0f;

        // ---- Filters (2 serial) ---------------------------------------------
        std::array<FilterParams, Config::MaxFilters> filters;

        // ---- EQ (3-band) ----------------------------------------------------
        std::array<EQBandParams, Config::MaxEQBands> eq;

        // ---- Fixed Envelopes (DAHDSR) ---------------------------------------
        EnvelopeParams  ampEG;
        EnvelopeParams  filterEG;
        EnvelopeParams  pitchEG;

        // ---- Flex Envelopes (SFZ v2 egN) ------------------------------------
        std::vector<FlexEGParams>   flexEGs;

        // ---- Fixed LFOs -----------------------------------------------------
        FixedLFOParams  ampLFO;
        FixedLFOParams  filterLFO;
        FixedLFOParams  pitchLFO;

        // ---- Flex LFOs (SFZ v2 lfoN) ----------------------------------------
        std::vector<FlexLFOParams>  flexLFOs;

        // ---- Effect sends ---------------------------------------------------
        float           effect1     = 0.0f;             // send level to fx bus 1
        float           effect2     = 0.0f;
        float           effect3     = 0.0f;
        float           effect4     = 0.0f;

        // ---- Wavetable (SFZ v2) ---------------------------------------------
        bool            oscillator          = false;
        int             oscillatorMode      = 0;
        int             oscillatorMulti     = 1;
        float           oscillatorPhase     = 0.0f;
        ModulatedFloat  oscillatorDetune    {0.0f};
        ModulatedFloat  oscillatorModDepth  {0.0f};
        int             oscillatorQuality   = 1;
        int             oscillatorTableSize = 0;

        // ---- Internal bookkeeping -------------------------------------------
        int             sourceGroupIndex    = -1;   // which <group> this came from
        int             sampleIndex         = -1;   // resolved sample index in SampleManager

        // Initialize amp envelope with sensible defaults
        Region()
        {
            ampEG.sustain = ModulatedFloat(100.0f);
            ampEG.release = ModulatedFloat(0.001f); // tiny release to avoid clicks
            filters[0].type = FilterType::None;
            filters[1].type = FilterType::None;
        }
    };

} // namespace sfz
