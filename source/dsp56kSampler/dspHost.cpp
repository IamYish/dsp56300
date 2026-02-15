#include "dspHost.h"
#include "dspProgram.h"

#include "dsp56kEmu/dsp.h"
#include "dsp56kEmu/memory.h"
#include "dsp56kEmu/peripherals.h"

#include <cmath>
#include <iostream>

namespace dsp56kSampler
{
	DspHost::DspHost()
	{
		m_memoryValidator = std::make_unique<dsp56k::DefaultMemoryValidator>();

		// Create memory with reasonable sizes:
		// P: 64K words, X/Y: 64K words each, bridged at 0x8000
		m_memory = std::make_unique<dsp56k::Memory>(*m_memoryValidator, 0x10000, 0x10000, 0x8000);

		// Create peripherals (DSP56362 on X bus, NOP on Y bus)
		m_periphX = std::make_unique<dsp56k::Peripherals56362>();
		m_periphY = std::make_unique<dsp56k::PeripheralsNop>();

		// Create DSP
		m_dsp = std::make_unique<dsp56k::DSP>(*m_memory, m_periphX.get(), m_periphY.get());
	}

	DspHost::~DspHost()
	{
		stop();
	}

	void DspHost::loadSineProgram(const float _frequency, const float _sampleRate)
	{
		m_sampleRate = _sampleRate;

		// Build and load the DSP program into memory
		DspProgram::build(*m_memory, _frequency, _sampleRate);

		// Reset DSP first (clears all registers and peripherals)
		m_dsp->resetHW();

		// Configure ESAI AFTER reset (reset clears peripheral registers)
		configureEsai();

		// Configure DSP registers AFTER reset (reset clears R0, M0, N0)
		configureRegisters(_frequency, _sampleRate);
	}

	void DspHost::configureEsai()
	{
		auto& esai = m_periphX->getEsai();

		// Configure Transmit Clock Control Register (TCCR)
		// Internal clocks, prescaler bypass
		const dsp56k::TWord tccr =
			(1 << dsp56k::Esai::M_THCKD) |    // Internal high-frequency clock
			(1 << dsp56k::Esai::M_TFSD) |      // Internal frame sync
			(1 << dsp56k::Esai::M_TCKD) |      // Internal clock
			(1 << dsp56k::Esai::M_TPSR);       // Prescaler bypass (divide by 1)
		esai.writeTransmitClockControlRegister(tccr);

		// Configure Transmit Control Register (TCR)
		// Enable TX0 and TX1, enable TX interrupt, stereo mode
		const dsp56k::TWord tcr =
			(1 << dsp56k::Esai::M_TIE) |       // TX interrupt enable
			(1 << dsp56k::Esai::M_TE0) |        // Enable transmitter 0
			(1 << dsp56k::Esai::M_TE1);         // Enable transmitter 1
		esai.writeTransmitControlRegister(tcr);

		// Transmit slot masks (enable all slots)
		esai.writeTSMA(0xFFFF);
		esai.writeTSMB(0xFFFF);

		// Set up the audio callback to inject empty RX frames (keeps the ESAI clock running)
		auto& esaiAudio = static_cast<dsp56k::Audio&>(esai);
		esaiAudio.setCallback([](dsp56k::Audio*) {}, 0);
	}

	void DspHost::configureRegisters(const float _frequency, const float _sampleRate)
	{
		// Set R0 to sine table base address
		m_dsp->writeReg(dsp56k::Reg_R0, dsp56k::TReg24(static_cast<int32_t>(DspProgram::SineTableBase)));

		// Set M0 to SineTableSize-1 for modulo addressing
		m_dsp->writeReg(dsp56k::Reg_M0, dsp56k::TReg24(static_cast<int32_t>(DspProgram::SineTableSize - 1)));

		// Set N0 to phase increment (step size through sine table)
		const float increment = _frequency * static_cast<float>(DspProgram::SineTableSize) / _sampleRate;
		auto step = static_cast<int32_t>(std::round(increment));
		if (step < 1) step = 1;
		m_dsp->writeReg(dsp56k::Reg_N0, dsp56k::TReg24(step));
	}

	void DspHost::start()
	{
		if (m_running)
			return;

		m_terminate = false;
		m_running = true;
		m_thread = std::thread(&DspHost::dspThread, this);
	}

	void DspHost::stop()
	{
		if (!m_running)
			return;

		m_terminate = true;
		m_periphX->terminate();
		if (m_thread.joinable())
			m_thread.join();
		m_running = false;
	}

	void DspHost::readAudio(float* _output, const uint32_t _numFrames)
	{
		auto& esai = m_periphX->getEsai();

		// Read stereo frames from the ESAI output ring buffer.
		// Each frame: slot 0 = {TX0, TX1, TX2, TX3, TX4, TX5}
		// We only care about TX0 (left) and TX1 (right).
		for (uint32_t i = 0; i < _numFrames; ++i)
		{
			esai.getAudioOutputs().waitNotEmpty();
			esai.getAudioOutputs().pop_front([&](dsp56k::Audio::TxFrame& _frame)
			{
				if (_frame.empty())
				{
					_output[i * 2 + 0] = 0.0f;
					_output[i * 2 + 1] = 0.0f;
					return;
				}

				_output[i * 2 + 0] = dsp56k::dsp2sample<float>(_frame[0][0]);  // TX0 = left
				_output[i * 2 + 1] = dsp56k::dsp2sample<float>(_frame[0][1]);  // TX1 = right
			});
		}
	}

	void DspHost::feedAudioInput(const uint32_t _numFrames)
	{
		auto& esai = m_periphX->getEsai();
		esai.writeEmptyAudioIn(_numFrames);
	}

	void DspHost::dspThread()
	{
		while (!m_terminate)
		{
			m_dsp->exec();
		}
	}
}
