# sfzSamplerLib - Core sampler engine (platform-independent)
# This library contains all SFZ parsing, sample loading, DSP, and voice
# management. It has no platform-specific audio/MIDI/GUI code, making it
# directly reusable for a VST3 build.

set(SFZSAMPLER_LIB_SOURCES
    # core
    core/types.h
    core/config.h
    core/ringBuffer.h

    # sfz parser and data model
    sfz/types.h
    sfz/opcodes.h
    sfz/opcodes.cpp
    sfz/region.h
    sfz/region.cpp
    sfz/instrument.h
    sfz/instrument.cpp
    sfz/parser.h
    sfz/parser.cpp
    sfz/curveDefinition.h

    # sample loading
    sample/sampleData.h
    sample/sampleData.cpp
    sample/wavLoader.h
    sample/wavLoader.cpp
    sample/sampleManager.h
    sample/sampleManager.cpp

    # dsp modules
    dsp/envelope.h
    dsp/envelope.cpp
    dsp/lfo.h
    dsp/lfo.cpp
    dsp/filter.h
    dsp/filter.cpp
    dsp/eq.h
    dsp/eq.cpp
    dsp/effects.h
    dsp/effects.cpp

    # engine
    engine/voice.h
    engine/voice.cpp
    engine/voiceManager.h
    engine/voiceManager.cpp
    engine/modMatrix.h
    engine/modMatrix.cpp
    engine/engine.h
    engine/engine.cpp
)

add_library(sfzSamplerLib STATIC ${SFZSAMPLER_LIB_SOURCES})

target_include_directories(sfzSamplerLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

set_property(TARGET sfzSamplerLib PROPERTY FOLDER "sfzSampler")
