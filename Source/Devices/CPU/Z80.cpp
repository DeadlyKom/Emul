#include "Z80.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME() FName(std::format("{}", ThisDeviceName))

namespace
{
	static const char* ThisDeviceName = "CPU Z80";
}

FCPU_Z80::FCPU_Z80()
	: FDevice(DEVICE_NAME(), EName::Z80, EDeviceType::CPU)
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
		Cycle_Reset();
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
		if (Registers.NMC != MachineCycle::None)
		{
			Registers.CC = 0;
			Registers.MC = Registers.NMC;
			Registers.NMC = MachineCycle::None;
			Registers.Step = DecoderStep::T1;

			if (Registers.CP.IsEmpty())
			{
				Command = [this](FCPU_Z80& CPU) -> void { Cycle_InstructionFetch(); };
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
