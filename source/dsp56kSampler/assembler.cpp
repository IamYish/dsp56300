#include "assembler.h"

#include "dsp56kEmu/memory.h"
#include "dsp56kEmu/disasm.h"
#include "dsp56kEmu/opcodes.h"

#include <sstream>
#include <cassert>

namespace dsp56kSampler
{
	void Assembler::org(const uint32_t _addr)
	{
		while (m_pc < _addr)
			nop();
	}

	void Assembler::writeToMemory(dsp56k::Memory& _mem) const
	{
		for (uint32_t i = 0; i < m_program.size(); ++i)
			_mem.set(dsp56k::MemArea_P, i, m_program[i]);
	}

	std::string Assembler::disassemble() const
	{
		dsp56k::Opcodes opcodes;
		dsp56k::Disassembler disasm(opcodes);
		std::string result;
		disasm.disassembleMemoryBlock(result, m_program, 0, false, true, true);
		return result;
	}

	void Assembler::emit(const dsp56k::TWord _word)
	{
		ensureSize(m_pc);
		m_program[m_pc] = _word;
		++m_pc;
	}

	void Assembler::emit(const dsp56k::TWord _word, const dsp56k::TWord _extensionWord)
	{
		emit(_word);
		emit(_extensionWord);
	}

	void Assembler::ensureSize(const uint32_t _addr)
	{
		if (_addr >= m_program.size())
			m_program.resize(_addr + 1, 0);
	}

	dsp56k::TWord Assembler::parallel(const dsp56k::TWord _move, const dsp56k::TWord _alu)
	{
		return (_move & 0xFFFF00) | (_alu & 0xFF);
	}

	// --- Basic Instructions ---

	void Assembler::nop()
	{
		emit(0x000000);
	}

	void Assembler::rti()
	{
		// RTI: "000000000000000000000100"
		emit(0x000004);
	}

	void Assembler::rts()
	{
		// RTS: "000000000000000000001100"
		emit(0x00000C);
	}

	void Assembler::stop()
	{
		emit(0x000087);
	}

	void Assembler::wait()
	{
		emit(0x000086);
	}

	void Assembler::reset()
	{
		emit(0x000084);
	}

	// --- Control Flow ---

	void Assembler::jmp(const uint32_t _addr)
	{
		// Jmp_xxx: "000011000000aaaaaaaaaaaa"
		// mask1 = 0x0C0000, field aaaaaaaaaaaa at bits 11-0
		assert(_addr <= 0xFFF && "JMP address must fit in 12 bits");
		emit(0x0C0000 | (_addr & 0xFFF));
	}

	void Assembler::jsr(const uint32_t _addr)
	{
		// Jsr_xxx: "000011010000aaaaaaaaaaaa"
		// mask1 = 0x0D0000, field aaaaaaaaaaaa at bits 11-0
		assert(_addr <= 0xFFF && "JSR address must fit in 12 bits");
		emit(0x0D0000 | (_addr & 0xFFF));
	}

	void Assembler::jcc(const CondCode _cc, const uint32_t _addr)
	{
		// Jcc_xxx: "00001110CCCCaaaaaaaaaaaa"
		// mask1 = 0x0E0000
		// CCCC at bits 15-12, aaaaaaaaaaaa at bits 11-0
		assert(_addr <= 0xFFF && "Jcc address must fit in 12 bits");
		emit(0x0E0000 | (static_cast<uint32_t>(_cc) << 12) | (_addr & 0xFFF));
	}

	// --- ALU with Move_Nop ---

	void Assembler::clr(const uint32_t _d)
	{
		// CLR: "????????????????0001d011" → lower 8 bits
		// Move_Nop: "0010000000000000????????" → upper 16 bits
		// d at bit 3
		const dsp56k::TWord alu = 0x13 | ((_d & 1) << 3);
		emit(parallel(0x200000, alu));
	}

	void Assembler::add(const uint32_t _jjj, const uint32_t _d)
	{
		// Add_SD: "????????????????0JJJd000"
		// JJJ at bits 6-4, d at bit 3
		const dsp56k::TWord alu = ((_jjj & 7) << 4) | ((_d & 1) << 3);
		emit(parallel(0x200000, alu));
	}

	void Assembler::sub(const uint32_t _jjj, const uint32_t _d)
	{
		// Sub_SD: "????????????????0JJJd100"
		// JJJ at bits 6-4, d at bit 3
		const dsp56k::TWord alu = ((_jjj & 7) << 4) | ((_d & 1) << 3) | 0x04;
		emit(parallel(0x200000, alu));
	}

	void Assembler::inc(const uint32_t _d)
	{
		// Inc: "00000000000000000000100d"
		// Non-parallel instruction
		emit(0x000008 | (_d & 1));
	}

	void Assembler::cmp(const uint32_t _jjj, const uint32_t _d)
	{
		// Cmp_S1S2: "????????????????0JJJd101"
		// JJJ at bits 6-4, d at bit 3
		const dsp56k::TWord alu = ((_jjj & 7) << 4) | ((_d & 1) << 3) | 0x05;
		emit(parallel(0x200000, alu));
	}

	// --- Moves ---

	void Assembler::move_imm8(const uint32_t _imm, const uint32_t _dd, const uint32_t _ddd)
	{
		// Move_xx: "001dddddiiiiiiii????????"
		// This is a parallel move: upper 16 bits are the move, lower 8 is ALU (NOP = 0x00)
		// ddddd = (dd << 3) | ddd at bits 20-16
		// iiiiiiii at bits 15-8
		const uint32_t ddddd = ((_dd & 3) << 3) | (_ddd & 7);
		const dsp56k::TWord moveOp = 0x200000 | (ddddd << 16) | ((_imm & 0xFF) << 8);
		emit(moveOp); // ALU part is NOP (0x00)
	}

	void Assembler::movec_imm8(const uint32_t _imm, const uint32_t _ddddd)
	{
		// Movec_xx: "00000101iiiiiiii101DDDDD"
		// iiiiiiii at bits 15-8, DDDDD at bits 4-0
		emit(0x050000 | ((_imm & 0xFF) << 8) | 0xA0 | (_ddddd & 0x1F));
	}

	void Assembler::movec_reg(const uint32_t _src_dddddd, const uint32_t _dst_ddddd)
	{
		// Movec_S1D2: "00000100W1eeeeee1o1DDDDD"
		// W=1 means read from register, W=0 means write to register
		// Actually for reg-to-reg: W=1, eeeeee=src, DDDDD=dst
		// Pattern: "00000100W1eeeeee1o1DDDDD"
		// W at bit 15, eeeeee at bits 13-8, DDDDD at bits 4-0
		// Fixed bits: bit 14=1, bit 7=1, bit 5=1
		emit(0x040000 | (1 << 15) | (1 << 14) | ((_src_dddddd & 0x3F) << 8) | 0xA0 | (_dst_ddddd & 0x1F));
	}

	void Assembler::movep_reg_to_xqq(const uint32_t _reg_dddddd, const uint32_t _xAddr)
	{
		// Movep_SXqq: "00000100W1dddddd1q0qqqqq"
		// W=0 for write to peripheral
		// dddddd at bits 13-8
		// q at bit 6, qqqqq at bits 4-0
		// Peripheral address: $FFFF80 + qq_addr (6 bits, split as q:qqqqq)
		const uint32_t qqAddr = _xAddr - 0xFFFF80;
		assert(qqAddr < 64 && "X:qq address out of range");
		const uint32_t q_hi = (qqAddr >> 5) & 1;   // bit 5 of address → Field_q
		const uint32_t q_lo = qqAddr & 0x1F;        // bits 4-0 → Field_qqqqq
		emit(0x040000 | (1 << 14) | ((_reg_dddddd & 0x3F) << 8) | (1 << 7) | (q_hi << 6) | q_lo);
	}

	void Assembler::movep_xqq_to_reg(const uint32_t _xAddr, const uint32_t _reg_dddddd)
	{
		// Movep_SXqq: "00000100W1dddddd1q0qqqqq"
		// W=1 for read from peripheral
		const uint32_t qqAddr = _xAddr - 0xFFFF80;
		assert(qqAddr < 64 && "X:qq address out of range");
		const uint32_t q_hi = (qqAddr >> 5) & 1;
		const uint32_t q_lo = qqAddr & 0x1F;
		emit(0x040000 | (1 << 15) | (1 << 14) | ((_reg_dddddd & 0x3F) << 8) | (1 << 7) | (q_hi << 6) | q_lo);
	}

	void Assembler::move_x_rn_to_reg(const uint32_t _rn, const uint32_t _dst_ddd, const uint32_t _aluOp)
	{
		// Movex_ea: "01dd0dddW1MMMRRR????????"
		// For reading X:(Rn)+ to register:
		// W=1 (read from memory), MMM=011 (post-increment), RRR=_rn
		// dd:0:ddd encodes destination register
		// dd at bits 22-21, ddd at bits 18-16
		// W at bit 15, MMM at bits 14-12, RRR at bits 11-9
		const uint32_t dd = (_dst_ddd >> 3) & 3;
		const uint32_t ddd = _dst_ddd & 7;
		emit(0x400000 | (dd << 21) | (ddd << 16) | (1 << 15) | (0x3 << 12) | ((_rn & 7) << 9) | (_aluOp & 0xFF));
	}

	void Assembler::move_y_rn_to_reg(const uint32_t _rn, const uint32_t _dst_ddd, const uint32_t _aluOp)
	{
		// Movey_ea: "01dd1dddW1MMMRRR????????"
		// Same as X but bit 19 set (ddd has bit 3 set for Y variant)
		// Actually: upper bits are "01dd1dddW1MMMRRR"
		// dd at bits 22-21, bit 19 is always 1 for Y, ddd at bits 18-16
		const uint32_t dd = (_dst_ddd >> 3) & 3;
		const uint32_t ddd = _dst_ddd & 7;
		emit(0x400000 | (dd << 21) | (1 << 19) | (ddd << 16) | (1 << 15) | (0x3 << 12) | ((_rn & 7) << 9) | (_aluOp & 0xFF));
	}

	void Assembler::movec_imm24(const uint32_t _imm24, const uint32_t _ddddd)
	{
		// Movec_ea with immediate data extension word
		// Movec_ea: "00000101W1MMMRRR0S1DDDDD"
		// For immediate mode: MMM=111, RRR=100 (immediate addressing mode)
		// W=1 (read into register), S=0 (X memory, but doesn't matter for immediate)
		// DDDDD at bits 4-0
		emit(0x050000 | (1 << 15) | (1 << 14) | (0x7 << 12) | (0x4 << 9) | (0 << 6) | 0x20 | (_ddddd & 0x1F),
			 _imm24 & 0xFFFFFF);
	}

	// --- Bit manipulation ---

	void Assembler::ori(const uint32_t _imm8, const uint32_t _ee)
	{
		// Ori: "00000000iiiiiiii111110EE"
		// iiiiiiii at bits 15-8, EE at bits 1-0
		emit(0x000000 | ((_imm8 & 0xFF) << 8) | 0x3C | (_ee & 3));
	}

	void Assembler::andi(const uint32_t _imm8, const uint32_t _ee)
	{
		// Andi: "00000000iiiiiiii101110EE"
		// iiiiiiii at bits 15-8, EE at bits 1-0
		emit(0x000000 | ((_imm8 & 0xFF) << 8) | 0xB8 | (_ee & 3));
	}

	void Assembler::bset_pp(const uint32_t _bit, const uint32_t _ppAddr, const uint32_t _space)
	{
		// Bset_pp: "0000101010pppppp0S1bbbbb"
		// pppppp at bits 13-8 (address offset from $FFFFC0)
		// S at bit 6, bbbbb at bits 4-0
		const uint32_t pp = _ppAddr - 0xFFFFC0;
		assert(pp < 64 && "pp address out of range");
		emit(0x0A8000 | ((pp & 0x3F) << 8) | ((_space & 1) << 6) | 0x20 | (_bit & 0x1F));
	}

	void Assembler::bclr_pp(const uint32_t _bit, const uint32_t _ppAddr, const uint32_t _space)
	{
		// Bclr_pp: "0000101010pppppp0S0bbbbb"
		const uint32_t pp = _ppAddr - 0xFFFFC0;
		assert(pp < 64 && "pp address out of range");
		emit(0x0A8000 | ((pp & 0x3F) << 8) | ((_space & 1) << 6) | (_bit & 0x1F));
	}

	// --- Loops ---

	void Assembler::do_imm(const uint32_t _count, const uint32_t _endAddr)
	{
		// Do_xxx: "00000110iiiiiiii1000hhhh"
		// iiiiiiii at bits 15-8 (lower 8 bits of count)
		// hhhh at bits 3-0 (upper 4 bits of count)
		// Extension word: absolute end address
		const uint32_t lo = _count & 0xFF;
		const uint32_t hi = (_count >> 8) & 0xF;
		emit(0x060000 | (lo << 8) | 0x80 | hi, _endAddr & 0xFFFFFF);
	}

	void Assembler::enddo()
	{
		// Enddo: "00000000000000001o0o1100"
		// The 'o' bits can be 0 or 1, use 0
		emit(0x00008C);
	}
}
