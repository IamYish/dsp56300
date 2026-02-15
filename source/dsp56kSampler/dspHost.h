#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace dsp56k
{
	class DSP;
	class Memory;
	class IPeripherals;
	class Peripherals56362;
	class PeripheralsNop;
	class DefaultMemoryValidator;
}

namespace dsp56kSampler
{
	// Wraps the DSP56300 emulator with DSP56362 peripherals.
	// Creates the DSP, memory, and peripherals, loads the sampler program,
	// and runs the DSP execution loop on a dedicated thread.
	// Audio output is collected from the ESAI ring buffer.
	class DspHost
	{
	public:
		DspHost();
		~DspHost();

		// Load the sine wave program and configure the DSP.
		// Call before start().
		void loadSineProgram(float _frequency = 440.0f, float _sampleRate = 48000.0f);

		// Start the DSP execution thread.
		void start();

		// Stop the DSP execution thread.
		void stop();

		// Read stereo audio output from ESAI.
		// Blocks until the requested number of frames is available.
		// Each frame is 2 floats (left, right).
		void readAudio(float* _output, uint32_t _numFrames);

		// Feed silence into ESAI input (needed to keep the clock running).
		void feedAudioInput(uint32_t _numFrames);

		bool isRunning() const { return m_running; }

		dsp56k::DSP& getDSP() { return *m_dsp; }
		dsp56k::Memory& getMemory() { return *m_memory; }
		dsp56k::Peripherals56362& getPeriphX() { return *m_periphX; }

	private:
		void dspThread();
		void configureEsai();
		void configureRegisters(float _frequency, float _sampleRate);

		std::unique_ptr<dsp56k::DefaultMemoryValidator> m_memoryValidator;
		std::unique_ptr<dsp56k::Memory> m_memory;
		std::unique_ptr<dsp56k::Peripherals56362> m_periphX;
		std::unique_ptr<dsp56k::PeripheralsNop> m_periphY;
		std::unique_ptr<dsp56k::DSP> m_dsp;

		std::thread m_thread;
		std::atomic<bool> m_running{false};
		std::atomic<bool> m_terminate{false};

		float m_sampleRate = 48000.0f;
	};
}
