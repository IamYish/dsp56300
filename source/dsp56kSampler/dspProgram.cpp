#include "dspProgram.h"

#include "dsp56kEmu/memory.h"
#include "dsp56kEmu/esai.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dsp56kSampler
{
	void DspProgram::build(dsp56k::Memory& _mem, const float _frequency, const float _sampleRate)
	{
		Assembler a;

		buildVectorTable(a);
		buildMainProgram(a);
		buildTxISR(a);

		a.writeToMemory(_mem);

		loadSineTable(_mem);
		loadPhaseParams(_mem, _frequency, _sampleRate);
	}

	void DspProgram::buildVectorTable(Assembler& _asm)
	{
		// P:$0000 - Reset vector
		_asm.org(Vec_Reset);
		_asm.jmp(MainProgram);
		_asm.nop();

		// P:$0038 - ESAI Transmit Data interrupt
		_asm.org(Vec_ESAI_TX);
		_asm.jsr(TxISR);
		_asm.nop();
	}

	void DspProgram::buildMainProgram(Assembler& _asm)
	{
		_asm.org(MainProgram);

		// ESAI configuration is done from C++ host before DSP starts.
		// We just need to unmask interrupts and enter the main loop.

		// Unmask all interrupts: clear I0 and I1 bits in MR (bits 0-1)
		// ANDI #$FC,MR
		_asm.andi(0xFC, EE_MR);

		// Main loop: spin waiting for interrupts
		const uint32_t mainLoop = _asm.label();
		_asm.jmp(mainLoop);
	}

	void DspProgram::buildTxISR(Assembler& _asm)
	{
		_asm.org(TxISR);

		// ESAI TX ISR - outputs one stereo sample from the sine table.
		//
		// R0 points into Y memory sine table with modulo addressing (M0 = SineTableSize-1).
		// N0 is the step size (controls frequency).
		// Each interrupt: read Y:(R0)+N0 into accumulator A, write to TX0 and TX1.
		//
		// R0, M0, N0 are configured from C++ before DSP starts.

		// MOVE Y:(R0)+N0,A  (read sine value, advance pointer)
		// Movey_ea: "01dd1dddW1MMMRRR????????"
		//   dest=A: dd:ddd = 01:110 = 0x0E (accumulator A in decode_dddddd)
		//   W=1 (read), MMM=001 ((Rn)+Nn), RRR=000 (R0)
		//   ALU NOP = 0x00
		const dsp56k::TWord movey_r0n0_a =
			(0 << 23) |   // '0'
			(1 << 22) |   // '1'
			(0 << 21) |   // dd=01
			(1 << 20) |
			(1 << 19) |   // '1' (Y memory)
			(1 << 18) |   // ddd=110
			(1 << 17) |
			(0 << 16) |
			(1 << 15) |   // W=1 (read)
			(1 << 14) |   // '1' (fixed)
			(0 << 13) |   // MMM=001 (post-increment by Nn)
			(0 << 12) |
			(1 << 11) |
			(0 << 10) |   // RRR=000 (R0)
			(0 << 9)  |
			(0 << 8)  |
			0x00;          // NOP ALU
		_asm.emit_raw(movey_r0n0_a);

		// MOVEP A,X:$FFFFA0  (write left channel to ESAI TX0)
		_asm.movep_reg_to_xqq(Reg_A, dsp56k::Esai::M_TX0);

		// MOVEP A,X:$FFFFA1  (write right channel to ESAI TX1)
		_asm.movep_reg_to_xqq(Reg_A, dsp56k::Esai::M_TX1);

		// Return from interrupt
		_asm.rti();
	}

	void DspProgram::loadSineTable(dsp56k::Memory& _mem)
	{
		for (uint32_t i = 0; i < SineTableSize; ++i)
		{
			const double phase = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(SineTableSize);
			const double value = std::sin(phase);

			// Convert to 24-bit signed fixed-point ($7FFFFF = +max, $800000 = -max)
			// Scale by 0.9 to avoid clipping
			auto sample = static_cast<int32_t>(value * 0.9 * 8388607.0);
			if (sample > 8388607) sample = 8388607;
			if (sample < -8388608) sample = -8388608;

			_mem.set(dsp56k::MemArea_Y, SineTableBase + i, static_cast<dsp56k::TWord>(sample) & 0xFFFFFF);
		}
	}

	void DspProgram::loadPhaseParams(dsp56k::Memory& _mem, const float _frequency, const float _sampleRate)
	{
		// Phase increment for the sine table walker.
		// For frequency f at sample rate sr with table size N:
		//   step = round(f * N / sr)
		// N0 register is configured from C++ to this value.
		const float increment = _frequency * static_cast<float>(SineTableSize) / _sampleRate;
		const auto step = static_cast<uint32_t>(std::round(increment));

		_mem.set(dsp56k::MemArea_X, PhaseAccum, 0);
		_mem.set(dsp56k::MemArea_X, PhaseIncr, step > 0 ? step : 1);
	}
}
