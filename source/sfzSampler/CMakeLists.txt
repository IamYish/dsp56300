# sfzSampler - SFZ v2 Sampler built on dsp56300
# Produces:
#   sfzSamplerLib  - static library (portable core, usable by VST3 later)
#   sfzSampler     - standalone Windows executable

add_subdirectory(lib)

if(WIN32)
    add_subdirectory(standalone)
endif()
