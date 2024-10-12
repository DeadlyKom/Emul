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
	else if (Registers.bInitiCPU)
	{
		Cycle_InitCPU();
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
		if (Registers.bInstrCycleDone || Registers.NMC != MachineCycle::None)
		{
			Registers.CC = 0;
			Registers.MC = Registers.bInstrCycleDone ? MachineCycle::M1 : Registers.NMC;
			Registers.NMC = MachineCycle::None;
			Registers.DSCP = DecoderStep::T1;
			Registers.bInstrCycleDone = false;
			const bool bIsCyclyM1 = Registers.MC == MachineCycle::M1;
			Registers.bOpcodeDecoded = !bIsCyclyM1;
			Execute_Tick = nullptr;

			if (!Registers.CP.IsEmpty())
			{
				Execute_Cycle = std::move(Registers.CP.Get());
			}
			else if (bIsCyclyM1)
			{
				Execute_Cycle = [this](FCPU_Z80& CPU) { Cycle_OpcodeFetch(CPU); };
			}
			else
			{
				Execute_Cycle = nullptr;
			}
		}
		if (Execute_Cycle) { Execute_Cycle(*this); }

		if (Registers.bOpcodeDecoded)
		{
			if (Registers.bInstrExeDone || !Execute_Tick)
			{
				Registers.DSTP = Registers.MC == MachineCycle::M1 ? DecoderStep::T3_H1 : DecoderStep::T1_H1;
				Registers.bInstrExeDone = false;
				if (!Registers.TP.IsEmpty())
				{
					Execute_Tick = std::move(Registers.TP.Get());
				}
			}
			if (Execute_Tick) { Execute_Tick(*this); }
		}
	}
	Registers.CC++;
}

void FCPU_Z80::Reset()
{
	memset(&Registers, 0, sizeof(Registers));
}

FRegisters FCPU_Z80::GetRegisters() const
{
	return Registers;
}
