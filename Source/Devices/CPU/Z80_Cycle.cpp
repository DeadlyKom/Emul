#include "Z80.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define INCREMENT_HALF() { CG->Increment(reinterpret_cast<uint32_t&>(Registers.Step)); }
#define COMPLETED()		 { Registers.Step = DecoderStep::Completed; }

void FCPU_Z80::DecodeAndExecuteFetchedInstruction(uint8_t Opcode)
{
#ifndef NDEBUG
	assert(false);
#endif
	Unprefixed[Opcode](*this);
}

void FCPU_Z80::InstructionFetch()
{
	switch (Registers.Step)
	{
	case DecoderStep::T1_H1:
		// the Program Counter is placed on the address bus at the beginning of the M1 cycle
		SB->SetActive(BUS_M1);
		SB->SetDataOnAddressBus(*Registers.PC);
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

		// clock states T3 and T4 of a fetch cycle are used to refresh dynamic memories
		// The CPU uses this time to decode and execute the fetched instruction so that no other concurrent operation can be performed.
	case DecoderStep::T3_H1:
		// the CPU samples the data from the data bus with the rising edge of the clock of state T3,
		// and this same edge is used by the CPU to turn off the RD and MREQ signals
		DecodeAndExecuteFetchedInstruction(/*opcode*/SB->GetDataOnDataBus());
		SB->SetInactive(BUS_M1);
		SB->SetInactive(BUS_MREQ);
		SB->SetInactive(BUS_RD);

		// during T3 and T4, the lower seven bits of the address bus contain a memory refresh address and the RFSH signal becomes active,
		// indicating that a refresh read of all dynamic memories must be performed.
		SB->SetActive(BUS_RFSH);
		// timer for setting RFSH signal inactive at the end of T4 clock cycle
		ADD_EVENT_(CG, 4, "set inactive RFSH at the end of T4 clock cycle",
			[&]()
			{
				SB->IsInactive(BUS_RFSH);
			});
		SB->SetDataOnAddressBus(Registers.IR.Get());
		INCREMENT_HALF();
		break;
	case DecoderStep::T3_H2:
		// one half clock cycle intro T3 later, the MREQ signals goes active.
		SB->SetActive(BUS_MREQ);
		Registers.IR.IncrementR();
		INCREMENT_HALF();
		break;

	case DecoderStep::T4_H1:
		INCREMENT_HALF();
		break;
	case DecoderStep::T4_H2:
		SB->SetInactive(BUS_MREQ);
		ADD_EVENT_(CG, 1, "increment PC",
			[&]()
			{
				++Registers.PC;
			});
		COMPLETED();
		break;
	}
}

void FCPU_Z80::MemoryReadCycle(uint16_t Address, Register8& Register, std::function<void(FCPU_Z80& CPU)>&& CompletedCallback /*= nullptr*/)
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
