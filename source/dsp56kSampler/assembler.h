#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dsp56kEmu/types.h"

namespace dsp56k { class Memory; }

namespace dsp56kSampler
{
	// Register encoding constants for the 6-bit DDDDDD field
	// (used in MOVEP, MOVEC, and other instructions)
	enum RegEncoding : uint32_t
	{
		Reg_X0   = 0x04,
		Reg_X1   = 0x05,
		Reg_Y0   = 0x06,
		Reg_Y1   = 0x07,
		Reg_A0   = 0x08,
		Reg_B0   = 0x09,
		Reg_A2   = 0x0A,
		Reg_B2   = 0x0B,
		Reg_A1   = 0x0C,
		Reg_B1   = 0x0D,
		Reg_A    = 0x0E,
		Reg_B    = 0x0F,
		Reg_R0   = 0x10,
		Reg_R1   = 0x11,
		Reg_R2   = 0x12,
		Reg_R3   = 0x13,
		Reg_R4   = 0x14,
		Reg_R5   = 0x15,
		Reg_R6   = 0x16,
		Reg_R7   = 0x17,
		Reg_N0   = 0x18,
		Reg_N1   = 0x19,
		Reg_N2   = 0x1A,
		Reg_N3   = 0x1B,
		Reg_N4   = 0x1C,
		Reg_N5   = 0x1D,
		Reg_N6   = 0x1E,
		Reg_N7   = 0x1F,
		Reg_M0   = 0x20,
		Reg_M1   = 0x21,
		Reg_M2   = 0x22,
		Reg_M3   = 0x23,
		Reg_M4   = 0x24,
		Reg_M5   = 0x25,
		Reg_M6   = 0x26,
		Reg_M7   = 0x27,
		Reg_SR   = 0x39,
		Reg_OMR  = 0x3A,
		Reg_SP   = 0x3B,
		Reg_LC   = 0x3F,
		Reg_LA   = 0x3E,
	};

	// JJJ field encoding for Data ALU Source Operands
	enum JJJEncoding : uint32_t
	{
		JJJ_NotD = 1,  // opposite accumulator
		JJJ_X    = 2,
		JJJ_Y    = 3,
		JJJ_X0   = 4,
		JJJ_Y0   = 5,
		JJJ_X1   = 6,
		JJJ_Y1   = 7,
	};

	// Condition codes for Jcc, Bcc, etc.
	enum CondCode : uint32_t
	{
		CC_CC = 0x0,   // carry clear (HS)
		CC_GE = 0x1,   // greater or equal
		CC_NE = 0x2,   // not equal
		CC_PL = 0x3,   // plus
		CC_NN = 0x4,   // not normalized
		CC_EC = 0x5,   // extension clear
		CC_LC = 0x6,   // limit clear
		CC_GT = 0x7,   // greater than
		CC_CS = 0x8,   // carry set (LO)
		CC_LT = 0x9,   // less than
		CC_EQ = 0xA,   // equal
		CC_MI = 0xB,   // minus
		CC_NR = 0xC,   // normalized
		CC_ES = 0xD,   // extension set
		CC_LS = 0xE,   // limit set
		CC_LE = 0xF,   // less or equal
	};

	// EE field encoding for control registers (ORI/ANDI)
	enum EEEncoding : uint32_t
	{
		EE_MR  = 0,    // Mode Register (upper byte of SR)
		EE_CCR = 1,    // Condition Code Register (lower byte of SR)
		EE_COM = 2,    // reserved (was OMR on older DSPs)
		EE_EOM = 3,    // reserved
	};

	// Minimal DSP56300 assembler that emits opcodes directly to a word buffer.
	// Provides dedicated methods for each instruction needed by the sampler.
	class Assembler
	{
	public:
		Assembler() = default;

		// Current program counter (next emit address)
		uint32_t pc() const { return m_pc; }
		void setPC(uint32_t _pc) { m_pc = _pc; }

		// Reserve space (fill with NOP)
		void org(uint32_t _addr);

		// Get the generated program
		const std::vector<dsp56k::TWord>& program() const { return m_program; }

		// Write program to DSP memory
		void writeToMemory(dsp56k::Memory& _mem) const;

		// Disassemble and return as string (for debugging)
		std::string disassemble() const;

		// --- Basic Instructions ---
		void nop();
		void rti();
		void rts();
		void stop();
		void wait();
		void reset();

		// --- Control Flow ---
		void jmp(uint32_t _addr);       // JMP absolute (12-bit short)
		void jsr(uint32_t _addr);       // JSR absolute (12-bit short)
		void jcc(CondCode _cc, uint32_t _addr);  // Jcc absolute (12-bit)

		// --- ALU with Move_Nop (no parallel move) ---
		void clr(uint32_t _d);          // CLR A or B (d=0 for A, d=1 for B)
		void add(uint32_t _jjj, uint32_t _d);    // ADD S,D (S from JJJ encoding)
		void sub(uint32_t _jjj, uint32_t _d);    // SUB S,D
		void inc(uint32_t _d);          // INC D
		void cmp(uint32_t _jjj, uint32_t _d);    // CMP S,D

		// --- Moves ---
		void move_imm8(uint32_t _imm, uint32_t _dd, uint32_t _ddd);  // MOVE #xx,D (dd:ddd = 5-bit reg encoding)
		void movec_imm8(uint32_t _imm, uint32_t _ddddd);            // MOVEC #xx,D (DDDDD encoding)
		void movec_reg(uint32_t _src_dddddd, uint32_t _dst_ddddd);  // MOVEC S,D

		// Move to/from X:qq (I/O short address $FFFF80-$FFFFBF)
		void movep_reg_to_xqq(uint32_t _reg_dddddd, uint32_t _xAddr);
		void movep_xqq_to_reg(uint32_t _xAddr, uint32_t _reg_dddddd);

		// Move with Rn addressing
		// MOVE X:(Rn)+,D or MOVE S,X:(Rn)+ (with post-increment)
		// These use the parallel move Movex_ea format
		void move_x_rn_to_reg(uint32_t _rn, uint32_t _dst_ddd, uint32_t _aluOp = 0);
		void move_y_rn_to_reg(uint32_t _rn, uint32_t _dst_ddd, uint32_t _aluOp = 0);

		// MOVE #xxxx,D with extension word (long immediate)
		void movec_imm24(uint32_t _imm24, uint32_t _ddddd);

		// --- Bit manipulation ---
		void ori(uint32_t _imm8, uint32_t _ee);   // ORI #xx,D (MR/CCR/OMR)
		void andi(uint32_t _imm8, uint32_t _ee);  // ANDI #xx,D

		// BSET/BCLR on I/O short address pp ($FFFFC0-$FFFFFF)
		void bset_pp(uint32_t _bit, uint32_t _ppAddr, uint32_t _space); // space: 0=X, 1=Y
		void bclr_pp(uint32_t _bit, uint32_t _ppAddr, uint32_t _space);

		// --- Loops ---
		void do_imm(uint32_t _count, uint32_t _endAddr);  // DO #xxx,end_addr
		void enddo();

		// --- Set a label at current PC for later reference ---
		uint32_t label() const { return m_pc; }

		// Emit a raw pre-computed opcode word
		void emit_raw(dsp56k::TWord _word) { emit(_word); }
		void emit_raw(dsp56k::TWord _word, dsp56k::TWord _ext) { emit(_word, _ext); }

	private:
		void emit(dsp56k::TWord _word);
		void emit(dsp56k::TWord _word, dsp56k::TWord _extensionWord);
		void ensureSize(uint32_t _addr);

		// Combine parallel move + ALU operation
		static dsp56k::TWord parallel(dsp56k::TWord _move, dsp56k::TWord _alu);

		std::vector<dsp56k::TWord> m_program;
		uint32_t m_pc = 0;
	};
}
