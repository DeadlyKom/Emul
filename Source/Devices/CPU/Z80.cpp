#include "Z80.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME() FName(std::format("{}", ThisDeviceName))
#define INCREMENT_HALF() { CG.Increment(reinterpret_cast<uint32_t&>(Registers.Step)); }
#define INCREMENT_FULL() { CG.IncrementByDiscreteness(reinterpret_cast<uint32_t&>(Registers.Step)); }

namespace DecoderStep
{
	enum Type : uint32_t
	{
		// full-clock tick
		T1 = 0,
		T2,
		T3,
		T4,
		T5,
		T6,

		// half-clock tick
		T1_H1 = 0,
		T1_H2,
		T2_H1,
		T2_H2,
		T3_H1,
		T3_H2,
		T4_H1,
		T4_H2,
		T5_H1,
		T5_H2,
		T6_H1,
		T6_H2,

		Completed = uint32_t(-1),
	};
}

namespace MachineCycle
{
	enum Type : int32_t
	{
		M1,
		M2,
		M3,
		M4,
		M5,
		M6,
	};

	Type& operator++(MachineCycle::Type& Value)
	{
		return reinterpret_cast<Type&>(++reinterpret_cast<int32_t&>(Value));
	}
	Type operator++(Type& Value, int32_t)
	{
		return ++Value;
	}
}

namespace
{
	static const char* ThisDeviceName = "CPU Z80";
}

FCPU_Z80::FCPU_Z80()
	: FDevice(DEVICE_NAME(), EDeviceType::CPU)
{}

void FCPU_Z80::Tick(FClockGenerator& CG, FSignalsBus& SB)
{
	// handle signals in this sequence:
	// 1. RESET
	// 2. BUSREQ
	// 3. NMI
	// 4. INT

	if (SB.IsActive(BUS_RESET))
	{
		// the address and data bus enter a high-impedance state
		SB.SetDataOnAddressBus(0xFFFF);
		SB.SetDataOnDataBus(0xFF);

		// all control output signals enter an inactive state
		SB.SetAllControlOutput(ESignalState::High);

		// skip half-clocs
		switch (CG.ToFullCycles(Registers.Step))
		{
		// reset the status registers as described in "Undocumented Z80"
		case DecoderStep::T1:
			Registers.IFF1 = false;
			Registers.IFF2 = false;
			Registers.IM   = 0x00;
			INCREMENT_FULL();
			break;

		case DecoderStep::T2:
			Registers.PC = 0x0000;
			Registers.IR = 0x0000;
			INCREMENT_FULL();
			break;

		case DecoderStep::T3:
			Registers.SP = 0xFFFF;
			Registers.AF = 0xFFFF;
			INCREMENT_FULL();
			break;

		default:
			// the processor is idle, waiting for the active RESET signal to complete
			Registers.Step = DecoderStep::Completed;
			break;
		}
	}
	else if (false)
	{
		// ToDo handle signals in this sequence:
		// 1.BUSREQ
		// 2.NMI
		// 3.INT
	}
	else
	{
		switch (Registers.MC)
		{
		case MachineCycle::M1:
			InstructionFetch(CG, SB);
			break;
		case MachineCycle::M2:
			break;
		case MachineCycle::M3:
			break;
		case MachineCycle::M4:
			break;
		case MachineCycle::M5:
			break;
		case MachineCycle::M6:
			break;
		}
	}
	Registers.CC++;
}

void FCPU_Z80::Reset()
{
	memset(&Registers, 0, sizeof(Registers));
}

void FCPU_Z80::InstructionFetch(FClockGenerator& CG, FSignalsBus& SB)
{
	// reset the internal counters
	if (Registers.Step == DecoderStep::Completed)
	{
		Registers.CC = 0;
		Registers.Step = DecoderStep::T1_H1;
	}

	switch (Registers.Step)
	{
	case DecoderStep::T1_H1:
		// The Program Counter is placed on the address bus at the beginning of the M1 cycle
		SB.SetActive(BUS_M1);
		SB.SetDataOnAddressBus(Registers.PC.Get());
		INCREMENT_HALF();
		break;

	case DecoderStep::T1_H2:
		// One half clock cycle later, the MREQ and RD signals goes active
		SB.SetActive(BUS_MREQ);
		SB.SetActive(BUS_RD);
		INCREMENT_HALF();
		break;

	case DecoderStep::T2_H1:
		// if the WAIT signal is active, wait for the next tick
		if (SB.IsInactive(BUS_WAIT))
		{
			INCREMENT_HALF();
		}
		break;

	case DecoderStep::T2_H2:
		INCREMENT_HALF();
		break;

		// Clock states T3 and T4 of a fetch cycle are used to refresh dynamic memories
		// The CPU uses this time to decode and execute the fetched instruction so that no other concurrent operation can be performed.
	case DecoderStep::T3_H1:
		// the CPU samples the data from the data bus with the rising edge of the clock of state T3,
		// and this same edge is used by the CPU to turn off the RD and MREQ signals
		Registers.Opcode = SB.GetDataOnDataBus();
		SB.SetInactive(BUS_M1);
		SB.SetInactive(BUS_MREQ);
		SB.SetInactive(BUS_RD);

		// During T3 and T4, the lower seven bits of the address bus contain a memory refresh address and the RFSH signal becomes active,
		// indicating that a refresh read of all dynamic memories must be performed.
		SB.SetActive(BUS_RFSH);
		// Timer for setting RFSH signal inactive at the end of T4 clock cycle
		CG.AddEvent(4,
			[&]()
			{
				SB.IsInactive(BUS_RFSH);
			}, "set inactive RFSH at the end of T4 clock cycle");
		SB.SetDataOnAddressBus(Registers.IR.Get());
		INCREMENT_HALF();
		break;

	case DecoderStep::T3_H2:
		// One half clock cycle intro T3 later, the MREQ signals goes active.
		SB.SetActive(BUS_MREQ);
		Registers.IR.IncrementR();
		INCREMENT_HALF();
		break;

	case DecoderStep::T4_H1:
		INCREMENT_HALF();
		break;

	case DecoderStep::T4_H2:
		SB.SetInactive(BUS_MREQ);
		Registers.Step = DecoderStep::Completed;
		CG.AddEvent(1,
			[&]()
			{ 
				Registers.MC++;
			}, "Finish clock cycle T4");
		break;
	}
}

void FCPU_Z80::MemoryReadCycle(FClockGenerator& CG, FSignalsBus& SB)
{
}
