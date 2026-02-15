cmake_minimum_required(VERSION 3.10)

set(SAMPLER_SOURCES
	assembler.cpp
	assembler.h
	dspHost.cpp
	dspHost.h
	dspProgram.cpp
	dspProgram.h
	main.cpp
)

add_executable(dsp56kSampler ${SAMPLER_SOURCES})

target_link_libraries(dsp56kSampler PRIVATE dsp56kEmu)

target_include_directories(dsp56kSampler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_property(TARGET dsp56kSampler PROPERTY CXX_STANDARD 17)
