#include "ULA.h"

#include "Devices/Device.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME() FName(std::format("{}", ThisDeviceName))

namespace
{
	static const char* ThisDeviceName = "ULA";
}

FULA::FULA(const FDisplayCycles& _DisplayCycles, double _Frequency)
	: FDevice(DEVICE_NAME(), NAME_ULA, EDeviceType::ControlUnit, _Frequency)
	, DisplayCycles(_DisplayCycles)
	, bInterruptLatch(false)
{
	DisplayWidth = DisplayCycles.BorderL + DisplayCycles.DisplayH + DisplayCycles.BorderR;
	DisplayHeight = DisplayCycles.BorderT + DisplayCycles.DisplayV + DisplayCycles.BorderB;

	Scanline = (DisplayCycles.FlybackH + DisplayCycles.BorderL + DisplayCycles.DisplayH + DisplayCycles.BorderR);
	Line = (DisplayCycles.FlybackV + DisplayCycles.BorderT + DisplayCycles.DisplayV + DisplayCycles.BorderB);
	Frame = Scanline * Line;

	DisplayData.resize(DisplayWidth * DisplayHeight);
}

void FULA::Tick()
{
	BusLogic();
	ALULogic();
}

void FULA::CalculateFrequency(double MainFrequency, uint32_t Sampling)
{
	FrequencyDivider = FMath::CeilLogTwo(FMath::RoundToInt32(MainFrequency / Frequency * Sampling));
}

void FULA::SetDisplayCycles(const FDisplayCycles& NewDisplayCycles)
{
	DisplayCycles = NewDisplayCycles;
}

void FULA::GetSpectrumDisplay(FSpectrumDisplay& OutputDisplay) const
{
	OutputDisplay.DisplayCycles = DisplayCycles;
	OutputDisplay.DisplayData = DisplayData;
}

void FULA::BusLogic()
{
	const ESignalState::Type RAM16 = !SB->GetSignal(BUS_A14) || SB->GetSignal(BUS_A15);
}

void FULA::ALULogic()
{
	const uint32_t FrameClock = (CG->GetClockCounter() >> FrequencyDivider) % Frame;

	const bool bIsInterrupt = FrameClock >= 13 && FrameClock <= 33;

	const bool bIsFlybackH = FrameClock < DisplayCycles.FlybackH;
	const bool bIsFlybackV = FrameClock < DisplayCycles.FlybackV * Scanline;
	const bool bIsFlyback = bIsFlybackH || bIsFlybackV;

	const uint32_t LocalFrameClock = FrameClock - DisplayCycles.FlybackV * Scanline;
	const uint32_t Y = bIsFlybackV ? 0 : LocalFrameClock / Scanline;
	const uint32_t X = (LocalFrameClock - Y * Scanline) >= DisplayCycles.FlybackH ? ((LocalFrameClock - Y * Scanline) - DisplayCycles.FlybackH) % DisplayWidth : 0;

	const bool bIsBorderH = X < DisplayCycles.BorderL || X >= (DisplayCycles.BorderL + DisplayCycles.DisplayH);
	const bool bIsBorderV = Y < DisplayCycles.BorderT || Y >= (DisplayCycles.BorderT + DisplayCycles.DisplayV);
	const bool bIsBorder = bIsBorderH || bIsBorderV;

	if (bIsInterrupt || bInterruptLatch)
	{
		if (bInterruptLatch != bIsInterrupt)
		{
			SB->SetSignal(BUS_INT, bInterruptLatch ? ESignalState::Low : ESignalState::High);
		}
		bInterruptLatch = bIsInterrupt;
	}
	else if (bIsFlyback)
	{
		// nothing
		return;
	}
	else if (bIsBorder)
	{
		// ToDo read port #FE
		const uint8_t Color = 7;// rand() % 8;
		DisplayData[Y * DisplayWidth + X] = Color;
	}
	else
	{
		// ToDo display
		//const uint8_t Color = rand() % 256;
		//DisplayData[Y * DisplayWidth + X] = Color;

		switch (X & 0x08)
		{
			case 0:
			{
				break;
			}
			case 1:
			{
				break;
			}
			case 2:
			{
				break;
			}
			case 3:
			{
				break;
			}
			case 4:
			{
				break;
			}
			case 5:
			{
				break;
			}
			case 6:
			{
				break;
			}
			case 7:
			{
				break;
			}
		}

		//const uint8_t AddressBus_Low =
	}
}
