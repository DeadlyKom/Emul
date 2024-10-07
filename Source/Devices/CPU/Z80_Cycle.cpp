#include "Z80.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define INCREMENT_HALF()	{ CG->Increment(reinterpret_cast<uint32_t&>(Registers.Step)); }
#define INCREMENT_HALF_IF()	{ if (Registers.Step != DecoderStep::Completed) { INCREMENT_HALF(); } }
#define COMPLETED()			{ Registers.Step = DecoderStep::Completed; }

void FCPU_Z80::Cycle_Reset()
{
	// the address and data bus enter a high-impedance state
	SB->SetDataOnAddressBus(0xFFFF);
	SB->SetDataOnDataBus(0xFF);

	// all control output signals enter an inactive state
	SB->SetAllControlOutput(ESignalState::High);

	// skip half-clocs
	switch (CG->ToFullCycles(Registers.Step))
	{
		// reset the status registers as described in "Undocumented Z80"
	case DecoderStep::T1:
		Registers.IFF1 = false;
		Registers.IFF2 = false;
		Registers.IM = 0x00;
		INCREMENT_HALF();
		break;

	case DecoderStep::T2:
		Registers.PC = 0x0000;
		Registers.IR = 0x0000;
		INCREMENT_HALF();
		break;

	case DecoderStep::T3:
		Registers.SP = 0xFFFF;
		Registers.AF = 0xFFFF;
		INCREMENT_HALF();
		break;

	default:
		// the processor is idle, waiting for the active RESET signal to complete
		if (Registers.Step != DecoderStep::Completed)
		{
			COMPLETED();
			ADD_EVENT_(CG, 1, "Reset completed", [&]() { Cycle_InstructionFetch(); });
		}
		break;
	}
}

void FCPU_Z80::DecodeAndAddInstruction(uint8_t Opcode)
{
#ifndef NDEBUG
	assert(Unprefixed[Opcode] != nullptr);
#endif
	Unprefixed[Opcode](*this);
}

void FCPU_Z80::CycleExecuteFetchedInstruction(uint8_t Opcode, DecoderStep::Type Step)
{
#ifndef NDEBUG
	assert(Unprefixed_Cycle[Opcode] != nullptr);
#endif
	Unprefixed_Cycle[Opcode](*this, Step);
}

void FCPU_Z80::Cycle_InstructionFetch()
{
	switch (Registers.Step)
	{
		case DecoderStep::T1_H1:
		{
			// the Program Counter is placed on the address bus at the beginning of the M1 cycle
			SB->SetActive(BUS_M1);
			SB->SetDataOnAddressBus(*Registers.PC);
			INCREMENT_HALF();
			break;
		}
		case DecoderStep::T1_H2:
		{
			// one half clock cycle later, the MREQ and RD signals goes active
			SB->SetActive(BUS_MREQ);
			SB->SetActive(BUS_RD);
			INCREMENT_HALF();
			break;
		}

		case DecoderStep::T2_H1:
		{
			// if the WAIT signal is active, wait for the next tick
			if (SB->IsInactive(BUS_WAIT))
			{
				INCREMENT_HALF();
			}
			break;
		}
		case DecoderStep::T2_H2:
		{
			INCREMENT_HALF();
			break;
		}

		// clock states T3 and T4 of a fetch cycle are used to refresh dynamic memories
		// The CPU uses this time to decode and execute the fetched instruction so that no other concurrent operation can be performed.
		case DecoderStep::T3_H1:
		{
			// the CPU samples the data from the data bus with the rising edge of the clock of state T3,
			// and this same edge is used by the CPU to turn off the RD and MREQ signals

			Registers.Opcode = /*opcode*/ SB->GetDataOnDataBus();
			DecodeAndAddInstruction(Registers.Opcode);
			CycleExecuteFetchedInstruction(Registers.Opcode, Registers.Step);

			SB->SetInactive(BUS_M1);
			SB->SetInactive(BUS_MREQ);
			SB->SetInactive(BUS_RD);

			// during T3 and T4, the lower seven bits of the address bus contain a memory refresh address and the RFSH signal becomes active,
			// indicating that a refresh read of all dynamic memories must be performed.
			SB->SetActive(BUS_RFSH);
			// timer for setting RFSH signal inactive at the end of T4 clock cycle
			ADD_EVENT_(CG, 4, "set inactive RFSH at the end of T4 clock cycle", [&]() { SB->IsInactive(BUS_RFSH); });
			SB->SetDataOnAddressBus(Registers.IR.Get());
			INCREMENT_HALF();
			break;
		}
		case DecoderStep::T3_H2:
		{
			CycleExecuteFetchedInstruction(Registers.Opcode, Registers.Step);
			// one half clock cycle intro T3 later, the MREQ signals goes active.
			SB->SetActive(BUS_MREQ);
			Registers.IR.IncrementR();
			INCREMENT_HALF();
			break;
		}

		case DecoderStep::T4_H1:
		{
			CycleExecuteFetchedInstruction(Registers.Opcode, Registers.Step);
			INCREMENT_HALF();
			break;
		}
		case DecoderStep::T4_H2:
		{
			CycleExecuteFetchedInstruction(Registers.Opcode, Registers.Step);
			SB->SetInactive(BUS_MREQ);
			INCREMENT_HALF_IF();
			break;
		}

		default:
		{
			CycleExecuteFetchedInstruction(Registers.Opcode, Registers.Step);
			INCREMENT_HALF_IF();
			break;
		}
	}
}

void FCPU_Z80::Cycle_MemoryReadCycle(uint16_t Address, Register8& Register, std::function<void(FCPU_Z80& CPU)>&& CompletedCallback /*= nullptr*/)
{
	switch (Registers.Step)
	{
	case DecoderStep::T1_H1:
		SB->SetDataOnAddressBus(Address);
		INCREMENT_HALF();
		break;
	case DecoderStep::T1_H2:
		// one half clock cycle later, the MREQ and RD signals goes active
		SB->SetActive(BUS_MREQ);
		SB->SetActive(BUS_RD);
		INCREMENT_HALF();
		break;

	case DecoderStep::T2_H1:
		// if the WAIT signal is active, wait for the next tick
		if (SB->IsInactive(BUS_WAIT))
		{
			INCREMENT_HALF();
		}
		break;
	case DecoderStep::T2_H2:
		INCREMENT_HALF();
		break;

	case DecoderStep::T3_H1:
		// the CPU samples the data from the data bus with the rising edge of the clock of state T3
		Register = SB->GetDataOnDataBus();
		INCREMENT_HALF();
		break;

	case DecoderStep::T3_H2:
		// one half clock cycle later, the MREQ and RD signals goes inactive
		SB->SetInactive(BUS_MREQ);
		SB->SetInactive(BUS_RD);
		if (CompletedCallback) CompletedCallback(*this);
		COMPLETED();
		break;
	}
}

void FCPU_Z80::Cycle_MemoryWriteCycle(uint16_t Address, Register8& Register, std::function<void(FCPU_Z80& CPU)>&& CompletedCallback /*= nullptr*/)
{
	switch (Registers.Step)
	{
	case DecoderStep::T1_H1:
		SB->SetDataOnAddressBus(Address);
		INCREMENT_HALF();
		break;
	case DecoderStep::T1_H2:
		SB->SetActive(BUS_MREQ);
		SB->SetDataOnDataBus(*Register);
		INCREMENT_HALF();
		break;

	case DecoderStep::T2_H1:
		// if the WAIT signal is active, wait for the next tick
		if (SB->IsInactive(BUS_WAIT))
		{
			INCREMENT_HALF();
		}
		break;
	case DecoderStep::T2_H2:
		SB->SetActive(BUS_WR);
		INCREMENT_HALF();
		break;

	case DecoderStep::T3_H1:
		INCREMENT_HALF();
		break;
	case DecoderStep::T3_H2:
		// one half clock cycle later, the MREQ and WR signals goes inactive
		SB->SetInactive(BUS_MREQ);
		SB->SetInactive(BUS_WR);
		if (CompletedCallback) CompletedCallback(*this);
		COMPLETED();
		break;
	}
}

void FCPU_Z80::Cycle_ALU_LoadWZ_AddWZ_UnloadWZ()
{
	// 1 tick, low register WZ stores relative offset
	// 2 tick, high byte of WZ register is loaded with 0x00 if offset is positive, 0xFF if negative
	// 3 tick, add WZ += PC (ALU)
	// 4 tick, unload from WZ.L to PC.L
	// 5 tick, unload from WZ.H to PC.H

	switch (Registers.Step)
	{
	case DecoderStep::T1_H1:
		INCREMENT_HALF();
		break;

	case DecoderStep::T2_H1:
		Registers.WZ.H = Registers.WZ.L.GetBit(7) ? 0xFF : 0x00;
		INCREMENT_HALF();
		break;

	case DecoderStep::T3_H1:
		Registers.WZ += Registers.PC;
		INCREMENT_HALF();
		break;

	case DecoderStep::T4_H1:
		Registers.PC.L = Registers.WZ.L;
		INCREMENT_HALF();
		break;

	case DecoderStep::T5_H1:
		Registers.PC.H = Registers.WZ.H;
		INCREMENT_HALF();
		break;
	case DecoderStep::T5_H2:
		COMPLETED();
		break;

	default:
		INCREMENT_HALF();
		break;
	}
}
