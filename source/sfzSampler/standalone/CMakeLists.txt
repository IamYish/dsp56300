# sfzSampler standalone executable (Windows 10/11)
# Links against sfzSamplerLib for the core engine.
# Provides WinMM audio output, WinMM MIDI input, and Dear ImGui GUI.

set(SFZSAMPLER_STANDALONE_SOURCES
    main.cpp

    # WinMM audio backend
    audio/winmmAudio.h
    audio/winmmAudio.cpp

    # WinMM MIDI backend
    midi/winmmMidi.h
    midi/winmmMidi.cpp

    # Dear ImGui GUI
    gui/gui.h
    gui/gui.cpp
)

add_executable(sfzSampler WIN32 ${SFZSAMPLER_STANDALONE_SOURCES})

target_link_libraries(sfzSampler
    PRIVATE
        sfzSamplerLib
        winmm          # WinMM for audio + MIDI
        dwmapi         # DWM for vsync / compositing
        d3d11          # Direct3D 11 for ImGui backend
        dxgi           # DXGI for ImGui backend
)

target_include_directories(sfzSampler
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# TODO: add imgui as a dependency once the submodule is added
# target_link_libraries(sfzSampler PRIVATE imgui)

set_property(TARGET sfzSampler PROPERTY FOLDER "sfzSampler")
