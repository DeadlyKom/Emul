#include "Z80.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define INCREMENT_TP_HALF()	{ CPU.GetClockGenerator().Increment(reinterpret_cast<uint32_t&>(CPU.Registers.DSTP)); }

static uint8_t UpdateFlags_SZ(uint8_t Value)
{
	return (Value != 0) ? (Value & Z80_SF) : Z80_ZF;
}

static void _def(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T3_H1:
		{
			break;
		}
		case DecoderStep::T3_H2:
		{
			break;
		}
		case DecoderStep::T4_H1:
		{
			break;
		}
		case DecoderStep::T4_H2:
		{
			CPU.Registers.bInstrCycleDone = true;
			CPU.Registers.bInstrExeDone = true;
			break;
		}
	}
	INCREMENT_TP_HALF();
}

static void _inc8(FCPU_Z80& CPU, Register8& Register)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T3_H1:
		{
			break;
		}
		case DecoderStep::T3_H2:
		{
			// update flags
			uint8_t& F = CPU.Registers.AF.L.Byte;
			uint8_t& Result = CPU.Registers.DataLatch;
			
			Result = Register.Byte + 1;
			uint8_t ResultFlags = UpdateFlags_SZ(Result) | (Result & (Z80_XF | Z80_YF)) | ((Result ^ Register.Byte) & Z80_HF);
			if (Result == 0x80)
			{
				ResultFlags |= Z80_VF;
			}
			F = ResultFlags | (F & Z80_CF);
			break;
		}

		case DecoderStep::T4_H1:
		{
			break;
		}
		case DecoderStep::T4_H2:
		{
			// update register
			uint8_t& Result = CPU.Registers.DataLatch;
			Register = Result;

			++CPU.Registers.PC;
			//COMPLETED();
			break;
		}
	}
}

static void _dec8(FCPU_Z80& CPU, Register8& Register)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T3_H1:
		{
			break;
		}
		case DecoderStep::T3_H2:
		{
			// update flags
			uint8_t& F = CPU.Registers.AF.L.Byte;
			uint8_t& Result = CPU.Registers.DataLatch;

			Result = Register.Byte - 1;
			uint8_t ResultFlags = Z80_NF | UpdateFlags_SZ(Result) | (Result & (Z80_XF | Z80_YF)) | ((Result ^ Register.Byte) & Z80_HF);
			if (Result == 0x7F)
			{
				ResultFlags |= Z80_VF;
			}
			F = ResultFlags | (F & Z80_CF);
			break;
		}

		case DecoderStep::T4_H1:
		{
			break;
		}
		case DecoderStep::T4_H2:
		{
			// update register
			uint8_t& Result = CPU.Registers.DataLatch;
			Register = Result;

			++CPU.Registers.PC;
			//COMPLETED();
			break;
		}
	}
}

// nop
void _00(FCPU_Z80& CPU)
{
	PUT_PIPELINE(TP, [](FCPU_Z80& CPU) -> void {  _def(CPU); });
}

// ld bc, nn
void _01_m1(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T4_H2:
		{
			CPU.Registers.NMC = MachineCycle::M2;
			break;
		}
	}
	INCREMENT_TP_HALF();
}
void _01_m2(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T2_H2:
		{
			if (CPU.GetSignalsBus().IsActive(BUS_WAIT))
			{
				CPU.Registers.DSTP = DecoderStep::T_WAIT;
			}
			break;
		}
		case DecoderStep::T_WAIT:
		{
			CPU.Registers.DSTP = DecoderStep::T2_H2;
			break;
		}
		case DecoderStep::T3_H2:
		{
			CPU.Registers.NMC = MachineCycle::M3;
			break;
		}
	}
	INCREMENT_TP_HALF();
}
void _01_m3(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T2_H2:
		{
			if (CPU.GetSignalsBus().IsActive(BUS_WAIT))
			{
				CPU.Registers.DSTP = DecoderStep::T_WAIT;
			}
			break;
		}
		case DecoderStep::T_WAIT:
		{
			CPU.Registers.DSTP = DecoderStep::T2_H2;
			break;
		}
		case DecoderStep::T3_H2:
		{
			CPU.Registers.bInstrCycleDone = true;
			CPU.Registers.bInstrExeDone = true;
			break;
		}
	}
	INCREMENT_TP_HALF();
}
void _01(FCPU_Z80& CPU)
{
	PUT_PIPELINE(CP, [](FCPU_Z80& CPU) -> void { CPU.Cycle_MemoryRead(*CPU.Registers.PC, CPU.Registers.BC.L); });
	PUT_PIPELINE(CP, [](FCPU_Z80& CPU) -> void { CPU.Cycle_MemoryRead(*CPU.Registers.PC, CPU.Registers.BC.H); });
	PUT_PIPELINE(TP, [](FCPU_Z80& CPU) -> void { _01_m1(CPU); });
	PUT_PIPELINE(TP, [](FCPU_Z80& CPU) -> void { _01_m2(CPU); });
	PUT_PIPELINE(TP, [](FCPU_Z80& CPU) -> void { _01_m3(CPU); });
}

// ld (bc), a
void _02_m1(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T4_H2:
		{
			CPU.Registers.NMC = MachineCycle::M4;
			break;
		}
	}
	INCREMENT_TP_HALF();
}
void _02_m4(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T2_H2:
		{
			if (CPU.GetSignalsBus().IsActive(BUS_WAIT))
			{
				CPU.Registers.DSTP = DecoderStep::T_WAIT;
			}
			break;
		}
		case DecoderStep::T_WAIT:
		{
			CPU.Registers.DSTP = DecoderStep::T2_H2;
			break;
		}
		case DecoderStep::T3_H2:
		{
			CPU.Registers.bInstrCycleDone = true;
			CPU.Registers.bInstrExeDone = true;
			break;
		}
	}
	INCREMENT_TP_HALF();
}
void _02(FCPU_Z80& CPU)
{
	PUT_PIPELINE(CP, [](FCPU_Z80& CPU) -> void { CPU.Cycle_MemoryWrite(*CPU.Registers.BC, CPU.Registers.AF.H); });
	PUT_PIPELINE(TP, [](FCPU_Z80& CPU) -> void { _02_m1(CPU); });
	PUT_PIPELINE(TP, [](FCPU_Z80& CPU) -> void { _02_m4(CPU); });
}

// inc bc
void _03(FCPU_Z80& CPU)
{}

void _03c(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T3_H1:
		{
			break;
		}
		case DecoderStep::T3_H2:
		{
			// update flags ?
			break;
		}

		case DecoderStep::T4_H1:
		{
			break;
		}
		case DecoderStep::T4_H2:
		{
			// read register bc and increment in alu
			break;
		}

		case DecoderStep::T5_H1:
		{
			break;
		}
		case DecoderStep::T5_H2:
		{
			// write to register bc
			++CPU.Registers.BC;
			break;
		}

		case DecoderStep::T6_H1:
		{
			break;
		}
		case DecoderStep::T6_H2:
		{
			// increment PC and end M1
			++CPU.Registers.PC;
			//COMPLETED();
			break;
		}
	}
}

// inc b
void _04(FCPU_Z80& CPU)
{}

void _04c(FCPU_Z80& CPU)
{
	_inc8(CPU, CPU.Registers.BC.H);
}

// dec b
void _05(FCPU_Z80& CPU)
{}

void _05c(FCPU_Z80& CPU)
{
	_dec8(CPU, CPU.Registers.BC.H);
}

// ld b, n
void _06(FCPU_Z80& CPU)
{
	PUT_PIPELINE(CP, [](FCPU_Z80& CPU) -> void { CPU.Cycle_MemoryRead(*CPU.Registers.PC, CPU.Registers.BC.H); });
}

// rlca
void _07(FCPU_Z80& CPU)
{}

void _07c(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T3_H1:
		{
			// update flags
			uint8_t& A = CPU.Registers.AF.H.Byte;
			uint8_t& F = CPU.Registers.AF.L.Byte;
			uint8_t& Result = CPU.Registers.DataLatch;

			Result = (A << 1) | (A >> 7);
			F = ((A >> 7) & Z80_CF) | (F & (Z80_SF | Z80_ZF | Z80_PF)) | (Result & (Z80_YF | Z80_XF));

			break;
		}
		case DecoderStep::T3_H2:
		{
			break;
		}

		case DecoderStep::T4_H1:
		{
			// update register
			uint8_t& A = CPU.Registers.AF.H.Byte;
			uint8_t& Result = CPU.Registers.DataLatch;
			A = Result;
			break;
		}
		case DecoderStep::T4_H2:
		{
			++CPU.Registers.PC;
			//COMPLETED();
			break;
		}
	}
}

// ex af, af'
void _08(FCPU_Z80& CPU)
{}

void _08c(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
	case DecoderStep::T3_H1:
	{
		break;
	}
	case DecoderStep::T3_H2:
	{
		// update flags
		break;
	}

	case DecoderStep::T4_H1:
	{
		break;
	}
	case DecoderStep::T4_H2:
	{
		// update register
		CPU.Registers.AF.Exchange(CPU.Registers.AF_);

		++CPU.Registers.PC;
		//COMPLETED();
		break;
	}
	}
}

// add hl, bc
void _09(FCPU_Z80& CPU)
{}

// ld a, (bc)
void _0a(FCPU_Z80& CPU)
{}

// dec bc
void _0b(FCPU_Z80& CPU)
{}

// inc c
void _0c(FCPU_Z80& CPU)
{}

// dec c
void _0d(FCPU_Z80& CPU)
{}

// ld c, n
void _0e(FCPU_Z80& CPU)
{}

// rrca
void _0f(FCPU_Z80& CPU)
{}

// djnz $
void _10(FCPU_Z80& CPU)
{}

// ld de, nn
void _11(FCPU_Z80& CPU)
{}

// ld (de), a
void _12(FCPU_Z80& CPU)
{}

// inc de
void _13(FCPU_Z80& CPU)
{}

// inc d
void _14(FCPU_Z80& CPU)
{}

// dec d
void _15(FCPU_Z80& CPU)
{}

// ld d, n
void _16(FCPU_Z80& CPU)
{}

// rla
void _17(FCPU_Z80& CPU)
{}

// jr $
void _18_m1(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T4_H2:
		{
			CPU.Registers.NMC = MachineCycle::M2;
			break;
		}
	}
	INCREMENT_TP_HALF();
}
void _18_m2(FCPU_Z80& CPU)
{
	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T3_H2:
		{
			CPU.Registers.NMC = MachineCycle::M3;
			break;
		}
	}
	INCREMENT_TP_HALF();
}
void _18_m3(FCPU_Z80& CPU)
{
	// 1 tick, low register WZ stores relative offset
	// 2 tick, high byte of WZ register is loaded with 0x00 if offset is positive, 0xFF if negative
	// 3 tick, add WZ += PC (ALU)
	// 4 tick, unload from WZ.L to PC.L
	// 5 tick, unload from WZ.H to PC.H

	switch (CPU.Registers.DSTP)
	{
		case DecoderStep::T2_H1:
		{
			CPU.Registers.WZ.H = CPU.Registers.WZ.L.GetBit(7) ? 0xFF : 0x00;
			break;
		}
		case DecoderStep::T3_H1:
		{
			CPU.Registers.WZ += CPU.Registers.PC;
			break;
		}
		case DecoderStep::T4_H1:
		{
			CPU.Registers.PC.L = CPU.Registers.WZ.L;
			break;
		}
		case DecoderStep::T5_H1:
		{
			CPU.Registers.PC.H = CPU.Registers.WZ.H;
			break;
		}
		case DecoderStep::T5_H2:
		{
			CPU.Registers.bInstrCycleDone = true;
			CPU.Registers.bInstrExeDone = true;
			break;
		}
	}
	INCREMENT_TP_HALF();
}
void _18(FCPU_Z80& CPU)
{
	PUT_PIPELINE(CP, [](FCPU_Z80& CPU) -> void { CPU.Cycle_MemoryRead(*CPU.Registers.PC, CPU.Registers.WZ.L); });
	PUT_PIPELINE(TP, [](FCPU_Z80& CPU) -> void { _18_m1(CPU); });
	PUT_PIPELINE(TP, [](FCPU_Z80& CPU) -> void { _18_m2(CPU); });
	PUT_PIPELINE(TP, [](FCPU_Z80& CPU) -> void { _18_m3(CPU); });
}

// add hl, de
void _19(FCPU_Z80& CPU)
{}

// ld a, (de)
void _1a(FCPU_Z80& CPU)
{}

// dec de
void _1b(FCPU_Z80& CPU)
{}

// inc e
void _1c(FCPU_Z80& CPU)
{}

// dec e
void _1d(FCPU_Z80& CPU)
{}

// ld e, n
void _1e(FCPU_Z80& CPU)
{}

// rra
void _1f(FCPU_Z80& CPU)
{}

// jr nz, $
void _20(FCPU_Z80& CPU)
{}

// ld hl, nn
void _21(FCPU_Z80& CPU)
{}

// ld (nn), hl
void _22(FCPU_Z80& CPU)
{}

// inc hl
void _23(FCPU_Z80& CPU)
{}

// inc h
void _24(FCPU_Z80& CPU)
{}

// dec h
void _25(FCPU_Z80& CPU)
{}

// ld h, n
void _26(FCPU_Z80& CPU)
{}

// daa
void _27(FCPU_Z80& CPU)
{}

// jr z, $
void _28(FCPU_Z80& CPU)
{}

// add hl, hl
void _29(FCPU_Z80& CPU)
{}

// ld hl, (nn)
void _2a(FCPU_Z80& CPU)
{}

// dec hl
void _2b(FCPU_Z80& CPU)
{}

// inc l
void _2c(FCPU_Z80& CPU)
{}

// dec l
void _2d(FCPU_Z80& CPU)
{}

// ld l, n
void _2e(FCPU_Z80& CPU)
{}

// cpl
void _2f(FCPU_Z80& CPU)
{}

// jr nc, $
void _30(FCPU_Z80& CPU)
{}

// ld sp, nn
void _31(FCPU_Z80& CPU)
{}

// ld (nn), a
void _32(FCPU_Z80& CPU)
{}

// inc sp
void _33(FCPU_Z80& CPU)
{}

// inc (hl)
void _34(FCPU_Z80& CPU)
{}

// dec (hl)
void _35(FCPU_Z80& CPU)
{}

// ld (hl), n
void _36(FCPU_Z80& CPU)
{}

// scf
void _37(FCPU_Z80& CPU)
{}

// jr c, $
void _38(FCPU_Z80& CPU)
{}

// add hl, sp
void _39(FCPU_Z80& CPU)
{}

// ld a, (nn)
void _3a(FCPU_Z80& CPU)
{}

// dec sp
void _3b(FCPU_Z80& CPU)
{}

// inc a
void _3c(FCPU_Z80& CPU)
{}

// dec a
void _3d(FCPU_Z80& CPU)
{}

// ld a, n
void _3e(FCPU_Z80& CPU)
{}

// ccf
void _3f(FCPU_Z80& CPU)
{}

// ld b, b
void _40(FCPU_Z80& CPU)
{}

// ld b, c
void _41(FCPU_Z80& CPU)
{}

// ld b, d
void _42(FCPU_Z80& CPU)
{}

// ld b, e
void _43(FCPU_Z80& CPU)
{}

// ld b, h
void _44(FCPU_Z80& CPU)
{}

// ld b, l
void _45(FCPU_Z80& CPU)
{}

// ld b, (hl)
void _46(FCPU_Z80& CPU)
{}

// ld b,a 
void _47(FCPU_Z80& CPU)
{}

// ld c, b
void _48(FCPU_Z80& CPU)
{}

// ld c, c
void _49(FCPU_Z80& CPU)
{}

// ld c, d
void _4a(FCPU_Z80& CPU)
{}

// ld c, e
void _4b(FCPU_Z80& CPU)
{}

// ld c, h
void _4c(FCPU_Z80& CPU)
{}

// ld c, l
void _4d(FCPU_Z80& CPU)
{}

// ld c, (hl)
void _4e(FCPU_Z80& CPU)
{}

// ld c, a
void _4f(FCPU_Z80& CPU)
{}

// ld d, b
void _50(FCPU_Z80& CPU)
{}

// ld d, c
void _51(FCPU_Z80& CPU)
{}

// ld d, d
void _52(FCPU_Z80& CPU)
{}

// ld d, e
void _53(FCPU_Z80& CPU)
{}

// ld d, h
void _54(FCPU_Z80& CPU)
{}

// ld d, l
void _55(FCPU_Z80& CPU)
{}

// ld d, (hl)
void _56(FCPU_Z80& CPU)
{}

// ld d, a
void _57(FCPU_Z80& CPU)
{}

// ld e, b
void _58(FCPU_Z80& CPU)
{}

// ld e, c
void _59(FCPU_Z80& CPU)
{}

// ld e, d
void _5a(FCPU_Z80& CPU)
{}

// ld e, e
void _5b(FCPU_Z80& CPU)
{}

// ld e, h
void _5c(FCPU_Z80& CPU)
{}

// ld e, l
void _5d(FCPU_Z80& CPU)
{}

// ld e, (hl)
void _5e(FCPU_Z80& CPU)
{}

// ld e, a
void _5f(FCPU_Z80& CPU)
{}

// ld h, b
void _60(FCPU_Z80& CPU)
{}

// ld h, c
void _61(FCPU_Z80& CPU)
{}

// ld h, d
void _62(FCPU_Z80& CPU)
{}

// ld h, e
void _63(FCPU_Z80& CPU)
{}

// ld h, h
void _64(FCPU_Z80& CPU)
{}

// ld h, l
void _65(FCPU_Z80& CPU)
{}

// ld h, (hl)
void _66(FCPU_Z80& CPU)
{}

// ld h, a
void _67(FCPU_Z80& CPU)
{}

// ld l, b
void _68(FCPU_Z80& CPU)
{}

// ld l, c
void _69(FCPU_Z80& CPU)
{}

// ld l, d
void _6a(FCPU_Z80& CPU)
{}

// ld l, e
void _6b(FCPU_Z80& CPU)
{}

// ld l, h
void _6c(FCPU_Z80& CPU)
{}

// ld l, l
void _6d(FCPU_Z80& CPU)
{}

// ld l, (hl)
void _6e(FCPU_Z80& CPU)
{}

// ld  l, a
void _6f(FCPU_Z80& CPU)
{}

// ld (hl), b
void _70(FCPU_Z80& CPU)
{}

// ld (hl), c
void _71(FCPU_Z80& CPU)
{}

// ld (hl), d
void _72(FCPU_Z80& CPU)
{}

// ld (hl), e
void _73(FCPU_Z80& CPU)
{}

// ld (hl), h
void _74(FCPU_Z80& CPU)
{}

// ld (hl), l
void _75(FCPU_Z80& CPU)
{}

// halt
void _76(FCPU_Z80& CPU)
{}

// ld (hl), a
void _77(FCPU_Z80& CPU)
{}

// ld a, b
void _78(FCPU_Z80& CPU)
{}

// ld a, c
void _79(FCPU_Z80& CPU)
{}

// ld a, d
void _7a(FCPU_Z80& CPU)
{}

// ld a, e
void _7b(FCPU_Z80& CPU)
{}

// ld a, h
void _7c(FCPU_Z80& CPU)
{}

// ld a, l
void _7d(FCPU_Z80& CPU)
{}

// ld a, (hl)
void _7e(FCPU_Z80& CPU)
{}

// ld a, a
void _7f(FCPU_Z80& CPU)
{}

// add a, b
void _80(FCPU_Z80& CPU)
{}

// add a, c
void _81(FCPU_Z80& CPU)
{}

// add a, d
void _82(FCPU_Z80& CPU)
{}

// add a, e
void _83(FCPU_Z80& CPU)
{}

// add a, h
void _84(FCPU_Z80& CPU)
{}

// add a,  l
void _85(FCPU_Z80& CPU)
{}

// add a, (hl)
void _86(FCPU_Z80& CPU)
{}

// add a, a
void _87(FCPU_Z80& CPU)
{}

// adc a, b
void _88(FCPU_Z80& CPU)
{}

// adc a, c
void _89(FCPU_Z80& CPU)
{}

// adc a, d
void _8a(FCPU_Z80& CPU)
{}

// adc a, e
void _8b(FCPU_Z80& CPU)
{}

// adc a, h
void _8c(FCPU_Z80& CPU)
{}

// adc a, l
void _8d(FCPU_Z80& CPU)
{}

// adc a, (hl)
void _8e(FCPU_Z80& CPU)
{}

// add a, a
void _8f(FCPU_Z80& CPU)
{}

// sub b
void _90(FCPU_Z80& CPU)
{}

// sub c
void _91(FCPU_Z80& CPU)
{}

// sub d
void _92(FCPU_Z80& CPU)
{}

// sub e
void _93(FCPU_Z80& CPU)
{}

// sub h
void _94(FCPU_Z80& CPU)
{}

// sub l
void _95(FCPU_Z80& CPU)
{}

// sub (hl)
void _96(FCPU_Z80& CPU)
{}

// sub a
void _97(FCPU_Z80& CPU)
{}

// sbc a, b
void _98(FCPU_Z80& CPU)
{}

// sbc a, c
void _99(FCPU_Z80& CPU)
{}

// sbc a, d
void _9a(FCPU_Z80& CPU)
{}

// sbc a, e
void _9b(FCPU_Z80& CPU)
{}

// sbc a, h
void _9c(FCPU_Z80& CPU)
{}

// sbc a, l
void _9d(FCPU_Z80& CPU)
{}

// sbc a, (hl)
void _9e(FCPU_Z80& CPU)
{}

// sbc a, a
void _9f(FCPU_Z80& CPU)
{}

// and b
void _a0(FCPU_Z80& CPU)
{}

// and c
void _a1(FCPU_Z80& CPU)
{}

// and d
void _a2(FCPU_Z80& CPU)
{}

// and e
void _a3(FCPU_Z80& CPU)
{}

// and h
void _a4(FCPU_Z80& CPU)
{}

// and l
void _a5(FCPU_Z80& CPU)
{}

// and (hl)
void _a6(FCPU_Z80& CPU)
{}

// and a
void _a7(FCPU_Z80& CPU)
{}

// xor b
void _a8(FCPU_Z80& CPU)
{}

// xor c
void _a9(FCPU_Z80& CPU)
{}

// xor d
void _aa(FCPU_Z80& CPU)
{}

// xor e
void _ab(FCPU_Z80& CPU)
{}

// xor h
void _ac(FCPU_Z80& CPU)
{}

// xor l
void _ad(FCPU_Z80& CPU)
{}

// xor (hl)
void _ae(FCPU_Z80& CPU)
{}

// xor a
void _af(FCPU_Z80& CPU)
{}

// or b
void _b0(FCPU_Z80& CPU)
{}

// or c
void _b1(FCPU_Z80& CPU)
{}

// or d
void _b2(FCPU_Z80& CPU)
{}

// or e
void _b3(FCPU_Z80& CPU)
{}

// or h
void _b4(FCPU_Z80& CPU)
{}

// or l
void _b5(FCPU_Z80& CPU)
{}

// or (hl)
void _b6(FCPU_Z80& CPU)
{}

// or a
void _b7(FCPU_Z80& CPU)
{}

// cp b
void _b8(FCPU_Z80& CPU)
{}

// cp c
void _b9(FCPU_Z80& CPU)
{}

// cp d
void _ba(FCPU_Z80& CPU)
{}

// cp e
void _bb(FCPU_Z80& CPU)
{}

// cp h
void _bc(FCPU_Z80& CPU)
{}

// cp l
void _bd(FCPU_Z80& CPU)
{}

// cp (hl)
void _be(FCPU_Z80& CPU)
{}

// cp a
void _bf(FCPU_Z80& CPU)
{}

// ret nz
void _c0(FCPU_Z80& CPU)
{}

// pop bc
void _c1(FCPU_Z80& CPU)
{}

// jp nz, nn
void _c2(FCPU_Z80& CPU)
{}

// jp nn
void _c3(FCPU_Z80& CPU)
{}

// call nz, nn
void _c4(FCPU_Z80& CPU)
{}

// push bc
void _c5(FCPU_Z80& CPU)
{}

// add a, n
void _c6(FCPU_Z80& CPU)
{}

// rst #00
void _c7(FCPU_Z80& CPU)
{}

// ret z
void _c8(FCPU_Z80& CPU)
{}

// ret
void _c9(FCPU_Z80& CPU)
{}

// jp z, nn
void _ca(FCPU_Z80& CPU)
{}

// opcode #CB
void _cb(FCPU_Z80& CPU)
{}

// call z, nn
void _cc(FCPU_Z80& CPU)
{}

// call nn
void _cd(FCPU_Z80& CPU)
{}

// adc a, n
void _ce(FCPU_Z80& CPU)
{}

// rst #08
void _cf(FCPU_Z80& CPU)
{}

// ret nc
void _d0(FCPU_Z80& CPU)
{}

// pop de
void _d1(FCPU_Z80& CPU)
{}

// jp nc, nn
void _d2(FCPU_Z80& CPU)
{}

// out (n), a
void _d3(FCPU_Z80& CPU)
{}

// call nc, nn
void _d4(FCPU_Z80& CPU)
{}

// push de
void _d5(FCPU_Z80& CPU)
{}

// sub n
void _d6(FCPU_Z80& CPU)
{}

// rst #10
void _d7(FCPU_Z80& CPU)
{}

// ret c
void _d8(FCPU_Z80& CPU)
{}

// exx
void _d9(FCPU_Z80& CPU)
{}

// jp c, nn
void _da(FCPU_Z80& CPU)
{}

// in a, (n)
void _db(FCPU_Z80& CPU)
{}

// call c, nn
void _dc(FCPU_Z80& CPU)
{}

// opcode DD
void _dd(FCPU_Z80& CPU)
{}

// sbc a, n
void _de(FCPU_Z80& CPU)
{}

// rst #18
void _df(FCPU_Z80& CPU)
{}

// ret po
void _e0(FCPU_Z80& CPU)
{}

// pop hl
void _e1(FCPU_Z80& CPU)
{}

// jp po, nn
void _e2(FCPU_Z80& CPU)
{}

// ex (sp), hl
void _e3(FCPU_Z80& CPU)
{}

// call po, nn
void _e4(FCPU_Z80& CPU)
{}

// push hl
void _e5(FCPU_Z80& CPU)
{}

// and n
void _e6(FCPU_Z80& CPU)
{}

// rst #20
void _e7(FCPU_Z80& CPU)
{}

// ret pe
void _e8(FCPU_Z80& CPU)
{}

// jp (hl)
void _e9(FCPU_Z80& CPU)
{}

// jp pe, nn
void _ea(FCPU_Z80& CPU)
{}

// ex de, hl
void _eb(FCPU_Z80& CPU)
{}

// call pe, nn
void _ec(FCPU_Z80& CPU)
{}

// opcode #ED
void _ed(FCPU_Z80& CPU)
{}

// xor n
void _ee(FCPU_Z80& CPU)
{}

// rst #28
void _ef(FCPU_Z80& CPU)
{}

// ret p
void _f0(FCPU_Z80& CPU)
{}

// pop af
void _f1(FCPU_Z80& CPU)
{}

// jp p, nn
void _f2(FCPU_Z80& CPU)
{}

//  di
void _f3(FCPU_Z80& CPU)
{}

// call p, nn
void _f4(FCPU_Z80& CPU)
{}

// push af
void _f5(FCPU_Z80& CPU)
{}

// or n
void _f6(FCPU_Z80& CPU)
{}

// rst #30
void _f7(FCPU_Z80& CPU)
{}

// ret m
void _f8(FCPU_Z80& CPU)
{}

// ld sp, hl
void _f9(FCPU_Z80& CPU)
{}

// jp m, nn
void _fa(FCPU_Z80& CPU)
{}

// ei
void _fb(FCPU_Z80& CPU)
{}

// call m, nn
void _fc(FCPU_Z80& CPU)
{}

// opcode #FD
void _fd(FCPU_Z80& CPU)
{}

// cp n
void _fe(FCPU_Z80& CPU)
{}

// rst #38
void _ff(FCPU_Z80& CPU)
{}

const CMD_FUNC FCPU_Z80::Unprefixed[256] =
{
	_00, _01, _02, _03, _04, _05, _06, _07, _08, _09, _0a, _0b, _0c, _0d, _0e, _0f,
	_10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _1a, _1b, _1c, _1d, _1e, _1f,
	_20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _2a, _2b, _2c, _2d, _2e, _2f,
	_30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _3a, _3b, _3c, _3d, _3e, _3f,
	_40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _4a, _4b, _4c, _4d, _4e, _4f,
	_50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _5a, _5b, _5c, _5d, _5e, _5f,
	_60, _61, _62, _63, _64, _65, _66, _67, _68, _69, _6a, _6b, _6c, _6d, _6e, _6f,
	_70, _71, _72, _73, _74, _75, _76, _77, _78, _79, _7a, _7b, _7c, _7d, _7e, _7f,
	_80, _81, _82, _83, _84, _85, _86, _87, _88, _89, _8a, _8b, _8c, _8d, _8e, _8f,
	_90, _91, _92, _93, _94, _95, _96, _97, _98, _99, _9a, _9b, _9c, _9d, _9e, _9f,
	_a0, _a1, _a2, _a3, _a4, _a5, _a6, _a7, _a8, _a9, _aa, _ab, _ac, _ad, _ae, _af,
	_b0, _b1, _b2, _b3, _b4, _b5, _b6, _b7, _b8, _b9, _ba, _bb, _bc, _bd, _be, _bf,
	_c0, _c1, _c2, _c3, _c4, _c5, _c6, _c7, _c8, _c9, _ca, _cb, _cc, _cd, _ce, _cf,
	_d0, _d1, _d2, _d3, _d4, _d5, _d6, _d7, _d8, _d9, _da, _db, _dc, _dd, _de, _df,
	_e0, _e1, _e2, _e3, _e4, _e5, _e6, _e7, _e8, _e9, _ea, _eb, _ec, _ed, _ee, _ef,
	_f0, _f1, _f2, _f3, _f4, _f5, _f6, _f7, _f8, _f9, _fa, _fb, _fc, _fd, _fe, _ff,
};
