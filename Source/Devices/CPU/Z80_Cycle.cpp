#include "Z80.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define INCREMENT_CP_HALF()	{ CG->Increment(reinterpret_cast<uint32_t&>(Registers.DSCP)); }

void FCPU_Z80::Cycle_Reset()
{
	switch (Registers.DSCP)
	{
		case DecoderStep::T1_H1:
		{
			Registers.NMC = MachineCycle::None;
			Registers.bIFF1 = false;
			Registers.bIFF2 = false;
			Registers.IM = 0x00;

			Registers.AF = 0xFFFF;
			Registers.PC = 0xFFFF;

			SB->SetInactive(BUS_BUSACK);
			SB->SetActive(BUS_MREQ);
			SB->SetActive(BUS_IORQ);
			SB->SetInactive(BUS_RD);
			SB->SetActive(BUS_WR);
			SB->SetInactive(BUS_HALT);

			SB->SetDataOnDataBus(0xFF);
			break;
		}
		case DecoderStep::T1_H2:
		{
			SB->SetInactive(BUS_RFSH);

			SB->SetDataOnAddressBus(0xFFFF);
			break;
		}
		case DecoderStep::T2_H1:
		{
			Registers.PC = 0x0000;
			Registers.IR = 0x0000;
			SB->SetInactive(BUS_IORQ);
			SB->SetInactive(BUS_WR);
			break;
		}
		case DecoderStep::T2_H2:
		{
			SB->SetInactive(BUS_MREQ);
			Registers.bInitiCPU = true;
			break;
		}
		default:
		{
			// the processor is idle, waiting for the active RESET signal to complete
		}
		break;
	}

	INCREMENT_CP_HALF();
}

void FCPU_Z80::Cycle_InitCPU()
{
	switch (Registers.DSCP)
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
			Registers.bInitiCPU = false;
			Registers.bInstrCycleDone = true;
			Registers.NMC = MachineCycle::M1;
			break;
		}
	}

	INCREMENT_CP_HALF();
}

void FCPU_Z80::OpcodeDecode()
{
	Registers.bOpcodeDecoded = true;

#ifndef NDEBUG
	assert(Unprefixed[Registers.Opcode] != nullptr);
#endif
	Unprefixed[Registers.Opcode](*this);
}

void FCPU_Z80::Cycle_OpcodeFetch(FCPU_Z80& CPU)
{
	switch (Registers.DSCP)
	{
		case DecoderStep::T1_H2:
		{
			SB->SetDataOnAddressBus(*Registers.PC);
			SB->SetActive(BUS_M1);
			break;
		}
		case DecoderStep::T2_H1:
		{
			SB->SetActive(BUS_MREQ);
			SB->SetActive(BUS_RD);
			++Registers.PC;
			break;
		}
		case DecoderStep::T2_H2:
		{
			if (SB->IsActive(BUS_WAIT))
			{
				Registers.DSCP = DecoderStep::T_WAIT;
			}
			break;
		}
		case DecoderStep::T_WAIT:
		{
			Registers.DSCP = DecoderStep::T2_H2;
			break;
		}
		case DecoderStep::T3_H1:
		{
			Registers.Opcode = /*read opcode*/ SB->GetDataOnDataBus();
			break;
		}
		case DecoderStep::T3_H2:
		{
			OpcodeDecode();

			SB->SetInactive(BUS_M1);
			SB->SetInactive(BUS_MREQ);
			SB->SetInactive(BUS_RD);

			SB->SetDataOnAddressBus(Registers.IR.Word);
			SB->SetActive(BUS_RFSH);
			ADD_EVENT_(CG, 4, "set inactive RFSH atafter 4 clock cycles", [&]() { SB->IsInactive(BUS_RFSH); });
			break;
		}
		case DecoderStep::T4_H1:
		{
			SB->SetActive(BUS_MREQ);
			ADD_EVENT_(CG, 2, "set inactive BUS_MREQ atafter 2 clock cycle", [&]() { SB->SetInactive(BUS_MREQ); });
			Registers.IR.IncrementR();
			break;
		}
		case DecoderStep::T4_H2:
		{
			CPU.Registers.Flags = Registers.AF.L;
			Registers.bOpcodeDecoded = false;
			break;
		}
	}

	INCREMENT_CP_HALF();
}

void FCPU_Z80::Cycle_MemoryRead(uint16_t Address, Register8& Register)
{
	switch (Registers.DSCP)
	{
		case DecoderStep::T1_H1:
		{
			break;
		}
		case DecoderStep::T1_H2:
		{
			SB->SetDataOnAddressBus(Address);
			break;
		}
		case DecoderStep::T2_H1:
		{
			SB->SetActive(BUS_MREQ);
			SB->SetActive(BUS_RD);
			break;
		}
		case DecoderStep::T2_H2:
		{
			if (SB->IsActive(BUS_WAIT))
			{
				Registers.DSCP = DecoderStep::T_WAIT;
			}
			break;
		}
		case DecoderStep::T_WAIT:
		{
			Registers.DSCP = DecoderStep::T2_H2;
			break;
		}
		case DecoderStep::T3_H1:
		{
			break;
		}
		case DecoderStep::T3_H2:
		{
			Register = SB->GetDataOnDataBus();
			ADD_EVENT_(CG, 1, "set inactive BUS_MREQ in next clock cycle", [&]() { SB->SetInactive(BUS_MREQ); });
			ADD_EVENT_(CG, 1, "set inactive BUS_RD in next clock cycle", [&]() { SB->SetInactive(BUS_RD); });
			break;
		}
	}
	INCREMENT_CP_HALF();
}

void FCPU_Z80::Cycle_MemoryWrite(uint16_t Address, Register8& Register)
{
	switch (Registers.DSCP)
	{
		case DecoderStep::T1_H1:
		{
			break;
		}
		case DecoderStep::T1_H2:
		{
			break;
		}
		case DecoderStep::T2_H1:
		{
			SB->SetDataOnAddressBus(Address);
			SB->SetDataOnDataBus(*Register);
			SB->SetActive(BUS_MREQ);
			ADD_EVENT_(CG, 1, "set active BUS_WR in next clock cycle", [&]() { SB->SetActive(BUS_WR); });
			break;
		}
		case DecoderStep::T2_H2:
		{
			if (SB->IsActive(BUS_WAIT))
			{
				Registers.DSCP = DecoderStep::T_WAIT;
			}
			break;
		}
		case DecoderStep::T_WAIT:
		{
			Registers.DSCP = DecoderStep::T2_H2;
			break;
		}
		case DecoderStep::T3_H1:
		{
			break;
		}
		case DecoderStep::T3_H2:
		{
			ADD_EVENT_(CG, 1, "set inactive BUS_MREQ in next clock cycle", [&]() { SB->SetInactive(BUS_MREQ); });
			ADD_EVENT_(CG, 1, "set inactive BUS_WR in next clock cycle", [&]() { SB->SetInactive(BUS_WR); });
			break;
		}
	}
	INCREMENT_CP_HALF();
}
