#include "Z80.h"
#include "Utils/Signal/Bus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME() FName(std::format("{}", ThisDeviceName))
#define ADD_STEP(a,b) (static_cast<DecoderStep::Type>(static_cast<int32_t>(a) + static_cast<int32_t>(b)))

namespace
{
	static const char* ThisDeviceName = "CPU Z80";
}

FCPU_Z80::FCPU_Z80(double _Frequency)
	: FDevice(DEVICE_NAME(), EName::Z80, EDeviceType::CPU, _Frequency)
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

			if (!Registers.CP.IsEmpty())
			{
				Execute_Cycle = std::move(Registers.CP.Get());
			}
			else if (/*is cycle M1*/Registers.MC == MachineCycle::M1)
			{
				Execute_Cycle =
					[this](FCPU_Z80& CPU)
					{
						Registers.bNextTickPipeline = Registers.bOpcodeDecoded && Registers.DSCP == DecoderStep::T4_H2;
						Cycle_OpcodeFetch(CPU);
					};
			}
			else
			{
				Execute_Cycle = nullptr;
			}
		}
		if (Execute_Cycle) { Execute_Cycle(*this); }

		if (!Registers.bInstrCompleted || Registers.bNextTickPipeline)
		{
			if (Registers.bNextTickPipeline)
			{
				Registers.DSTP = Registers.MC == MachineCycle::M1 ? DecoderStep::T4_H2 : 
					ADD_STEP(DecoderStep::T1_H1, (Registers.DSTP >= DecoderStep::OLP1_H1 ? Registers.DSTP - DecoderStep::OLP1_H1 : 0));
				Registers.bNextTickPipeline = false;
				Registers.bInstrCompleted = false;

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

void FCPU_Z80::CalculateFrequency(double MainFrequency, uint32_t Sampling)
{
	FrequencyDivider = FMath::CeilLogTwo(FMath::RoundToInt32(MainFrequency / Frequency));
}

bool FCPU_Z80::Flush()
{
	while (!Registers.bInstrCompleted)
	{
		if (!Execute_Tick)
		{
			return false;
		}

		Execute_Tick(*this);
		Registers.CC++;
	}
	return true;
}

double FCPU_Z80::GetFrequency() const
{
	return Frequency;
}

FRegisters FCPU_Z80::GetRegisters() const
{
	return Registers;
}
