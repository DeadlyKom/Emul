#include "Z80.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME() FName(std::format("{}", ThisDeviceName))
#define INCREMENT_FULL() { CG->IncrementByDiscreteness(reinterpret_cast<uint32_t&>(Registers.Step)); }

namespace
{
	static const char* ThisDeviceName = "CPU Z80";
}

FCPU_Z80::FCPU_Z80()
	: FDevice(DEVICE_NAME(), EDeviceType::CPU)
{}

void FCPU_Z80::Tick()
{
	// handle signals in this sequence:
	// 1. RESET
	// 2. BUSREQ
	// 3. NMI
	// 4. INT

	if (SB->IsActive(BUS_RESET))
	{
		// the address and data bus enter a high-impedance state
		SB->SetDataOnAddressBus(0xFFFF);
		SB->SetDataOnDataBus(0xFF);

		// all control output signals enter an inactive state
		SB->SetAllControlOutput(ESignalState::High);

		// skip half-clocs
		switch (Registers.Step)
		{
		// reset the status registers as described in "Undocumented Z80"
		case DecoderStep::T1_H1:
			Registers.IFF1 = false;
			Registers.IFF2 = false;
			Registers.IM   = 0x00;
			INCREMENT_FULL();
			break;

		case DecoderStep::T2_H1:
			Registers.PC = 0x0000;
			Registers.IR = 0x0000;
			INCREMENT_FULL();
			break;

		case DecoderStep::T3_H1:
			Registers.SP = 0xFFFF;
			Registers.AF = 0xFFFF;
			INCREMENT_FULL();
			break;

		default:
			// the processor is idle, waiting for the active RESET signal to complete
			if (Registers.Step != DecoderStep::Completed)
			{
				Registers.Step = DecoderStep::Completed;
				Registers.CP.Put(
					[this](FCPU_Z80& CPU) -> void
					{
						InstructionFetch();
					});
			}
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
		// reset the internal counters
		if (Registers.Step == DecoderStep::Completed)
		{
			Registers.MC++;
			Registers.CC = 0;
			Registers.Step = DecoderStep::T1_H1;

			if (Registers.CP.IsEmpty())
			{
				Registers.MC = MachineCycle::M1;

				Command = [this](FCPU_Z80& CPU) -> void
					{
						InstructionFetch();
					};
			}
			else
			{
				Command = Registers.CP.Get();
			}
		}

		if (Command)
		{
			Command(*this);
		}
	}
	Registers.CC++;
}

void FCPU_Z80::Reset()
{
	memset(&Registers, 0, sizeof(Registers));
}
