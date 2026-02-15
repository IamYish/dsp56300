#pragma once

#include "assembler.h"

namespace dsp56k
{
	class Memory;
}

namespace dsp56kSampler
{
	// Generates a minimal DSP56300 program that outputs a stereo sine wave
	// through the ESAI interface at 48kHz.
	//
	// Memory layout:
	//   P:$0000-$003F   Interrupt vector table
	//   P:$0040-$00FF   Main program (boot, ESAI init, main loop)
	//   P:$0100-$01FF   ESAI TX interrupt service routine
	//   Y:$0000-$00FF   Sine lookup table (256 entries, 24-bit fixed-point)
	//   X:$0000         Phase accumulator (current position in sine table)
	//   X:$0001         Phase increment (determines frequency)
	//
	class DspProgram
	{
	public:
		// Sine table parameters
		static constexpr uint32_t SineTableBase = 0x000000;  // Y memory
		static constexpr uint32_t SineTableSize = 256;

		// Voice state in X memory
		static constexpr uint32_t PhaseAccum = 0x000000;     // X memory
		static constexpr uint32_t PhaseIncr  = 0x000001;     // X memory

		// Program addresses
		static constexpr uint32_t VectorTable = 0x000000;
		static constexpr uint32_t MainProgram = 0x000040;
		static constexpr uint32_t TxISR       = 0x000100;

		// ESAI interrupt vector addresses (DSP56362)
		static constexpr uint32_t Vec_Reset     = 0x0000;
		static constexpr uint32_t Vec_ESAI_TX   = 0x0038;

		// Build the complete DSP program and write it to memory.
		// Also loads the sine table into Y memory and initial
		// phase parameters into X memory.
		static void build(dsp56k::Memory& _mem, float _frequency = 440.0f, float _sampleRate = 48000.0f);

	private:
		// Generate the interrupt vector table
		static void buildVectorTable(Assembler& _asm);

		// Generate the main program (ESAI initialization + main loop)
		static void buildMainProgram(Assembler& _asm);

		// Generate the ESAI TX ISR (sine wave output)
		static void buildTxISR(Assembler& _asm);

		// Load sine table into Y memory
		static void loadSineTable(dsp56k::Memory& _mem);

		// Set initial phase parameters in X memory
		static void loadPhaseParams(dsp56k::Memory& _mem, float _frequency, float _sampleRate);
	};
}
