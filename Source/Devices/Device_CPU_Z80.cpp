#include "Device_CPU_Z80.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

namespace
{
	static const FName CPU_Z80_DeviceName = "CPU Z80";
}

FCPU_Z80::FCPU_Z80()
	: FDevice(CPU_Z80_DeviceName, EDeviceType::CPU)
{}

void FCPU_Z80::Tick(FClockGenerator& CG, uint64_t& InOutISB)
{
	// handle signals in this sequence:
	// 1. RESET
	// 2. BUSREQ
	// 3. NMI
	// 4. INT

	if (BUS_IS_ACTIVE_RESET(InOutISB))
	{
		// the address and data bus enter a high-impedance state
		BUS_SET_ADR(InOutISB, 0xFFFF);
		BUS_SET_DATA(InOutISB, 0xFF);

		// all control output signals enter an inactive state
		InOutISB |= CTRL_OUTPUT_MASK;

		// skip half-clocs
		switch (Registers.Step >> 1)
		{
		case T1:
			Registers.IFF1 = false;
			Registers.IFF2 = false;
			Registers.IM   = 0x00;
			Registers.Step++;
			break;

		case T2:
			Registers.PC = 0x0000;
			Registers.IR = 0x0000;
			Registers.Step++;
			break;

		case T3:
			Registers.SP = 0xFFFF;
			Registers.AF = 0xFFFF;
			Registers.Step++;
			break;

		default:
			// the processor is idle, waiting for the active RESET signal to complete
			Registers.Step = CTC;
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
		case M1:
			InstructionOpcodeFetch(CG, InOutISB);
			break;
		case M2:
			break;
		case M3:
			break;
		case M4:
			break;
		case M5:
			break;
		case M6:
			break;
		}
	}
	Registers.CC++;
}

void FCPU_Z80::Reset()
{
	memset(&Registers, 0, sizeof(Registers));

	// reset state as described in 'The Undocumented Z80 Documented'
}

void FCPU_Z80::InstructionOpcodeFetch(FClockGenerator& CG, uint64_t& InOutISB)
{
	// reset the internal counters
	if (Registers.Step == CTC)
	{
		Registers.CC = 0;
		Registers.Step = T1;
	}

	switch (Registers.Step)
	{
	case T1_H1:
		// The Program Counter is placed on the address bus at the beginning of the M1 cycle
		BUS_SET_ACTIVE_SIGNAL(InOutISB, BUS_M1);
		BUS_SET_ADR(InOutISB, Registers.PC.Get());
		Registers.Step++;
		break;

	case T1_H2:
		// One half clock cycle later, the MREQ and RD signals goes active
		BUS_SET_ACTIVE_SIGNAL(InOutISB, BUS_MREQ | BUS_RD);
		Registers.Step++;
		break;

	case T2_H1:
		// if the WAIT signal is active, wait for the next tick
		if (BUS_IS_INACTIVE_WAIT(InOutISB))
		{
			Registers.Step++;
		}
		break;

	case T2_H2:
		Registers.Step++;
		break;

		// Clock states T3 and T4 of a fetch cycle are used to refresh dynamic memories
		// The CPU uses this time to decode and execute the fetched instruction so that no other concurrent operation can be performed.
	case T3_H1:
		// the CPU samples the data from the data bus with the rising edge of the clock of state T3,
		// and this same edge is used by the CPU to turn off the RD and MREQ signals
		Registers.Opcode = BUS_GET_DATA(InOutISB);
		BUS_SET_INACTIVE_SIGNAL(InOutISB, BUS_M1 | BUS_MREQ | BUS_RD);

		// During T3 and T4, the lower seven bits of the address bus contain a memory refresh address and the RFSH signal becomes active,
		// indicating that a refresh read of all dynamic memories must be performed.
		BUS_SET_ACTIVE_SIGNAL(InOutISB,  BUS_RFSH);
		// Timer for setting RFSH signal inactive at the end of T4 clock cycle
		CG.AddEvent(4,
			[&]()
			{
				BUS_SET_INACTIVE_SIGNAL(InOutISB, BUS_RFSH);
			}, "set inactive RFSH at the end of T4 clock cycle");
		BUS_SET_DATA(InOutISB, Registers.IR.Get());
		Registers.Step++;
		break;

	case T3_H2:
		// One half clock cycle intro T3 later, the MREQ signals goes active.
		BUS_SET_ACTIVE_SIGNAL(InOutISB, BUS_MREQ);
		Registers.IR.IncrementR();
		Registers.Step++;
		break;

	case T4_H1:
		Registers.Step++;
		break;

	case T4_H2:
		BUS_SET_INACTIVE_SIGNAL(InOutISB, BUS_MREQ);
		Registers.Step = CTC;
		CG.AddEvent(1,
			[&]()
			{ 
				Registers.MC++; 
			}, "Finish clock cycle T4");
		break;
	}
}
