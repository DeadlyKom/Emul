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
	, bDrawPixels(false)
	, bFlipFlopFlash(false)
	, bIsInterrupt(false)
	, bInterruptLatch(false)
	, FlashCounter(32)
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
	const uint64_t CC = CG->GetClockCounter();
	const uint32_t FrameClock = ( CC >> FrequencyDivider) % Frame;
	const bool bIsFirst = !(CC & 0x01);

	if (bIsFirst) BusLogic(FrameClock);
	ULALogic(FrameClock);
	if (bIsFirst) DrawLogic(FrameClock);
}

bool FULA::MainTick()
{
	Tick();
	return true;
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

void FULA::BusLogic(uint32_t FrameClock)
{
	const ESignalState::Type RAM16 = !SB->GetSignal(BUS_A14) || SB->GetSignal(BUS_A15);

	bIsInterrupt = FrameClock >= 13 && FrameClock <= 33;
	if (bIsInterrupt || bInterruptLatch)
	{
		if (bInterruptLatch != bIsInterrupt)
		{
			SB->SetSignal(BUS_INT, bInterruptLatch ? ESignalState::High : ESignalState::Low);
			if (!--FlashCounter)
			{
				FlashCounter = 32;
				bFlipFlopFlash = !bFlipFlopFlash;
			}
		}
		bInterruptLatch = bIsInterrupt;
	}
}

void FULA::ULALogic(uint32_t FrameClock)
{
	//const uint32_t FrameClock = (CG->GetClockCounter() >> FrequencyDivider) % Frame;

	/*const bool bIsInterrupt = FrameClock >= 13 && FrameClock <= 33;*/

	const bool bIsFlybackH = FrameClock < DisplayCycles.FlybackH;
	const bool bIsFlybackV = FrameClock < DisplayCycles.FlybackV * Scanline;
	bIsFlyback = bIsFlybackH || bIsFlybackV;

	const uint32_t LocalFrameClock = FrameClock - DisplayCycles.FlybackV * Scanline;
	Y = bIsFlybackV ? 0 : LocalFrameClock / Scanline;
	X = (LocalFrameClock - Y * Scanline) >= DisplayCycles.FlybackH ? ((LocalFrameClock - Y * Scanline) - DisplayCycles.FlybackH) % DisplayWidth : 0;

	const bool bIsBorderH = X < DisplayCycles.BorderL || X >= (DisplayCycles.BorderL + DisplayCycles.DisplayH);
	const bool bIsBorderV = Y < DisplayCycles.BorderT || Y >= (DisplayCycles.BorderT + DisplayCycles.DisplayV);
	bIsBorder = bIsBorderH || bIsBorderV;

	//if (bIsInterrupt || bInterruptLatch)
	//{
	//	if (bInterruptLatch != bIsInterrupt)
	//	{
	//		SB->SetSignal(BUS_INT, bInterruptLatch ? ESignalState::Low : ESignalState::High);
	//		if (!--FlashCounter)
	//		{
	//			FlashCounter = 32;
	//			bFlipFlopFlash = !bFlipFlopFlash;
	//		}
	//	}
	//	bInterruptLatch = bIsInterrupt;
	//}
	if (bIsInterrupt || bInterruptLatch)
	{
		// nothing
		return;
	}
	else if (bIsFlyback)
	{
		// nothing
		return;
	}
	else if (/*!bDrawPixels &&*/ bIsBorder)
	{
		// ToDo read port #FE
		const uint8_t Color = 15;
		DisplayData[Y * DisplayWidth + X] = Color;
	}
	else
	{
		// ToDo display
		//const uint8_t Color = rand() % 256;
		//DisplayData[Y * DisplayWidth + X] = Color;

		VideoFetch();
	}
}

void FULA::VideoFetch()
{
	const int32_t x = X - DisplayCycles.BorderL;
	const int32_t y = Y - DisplayCycles.BorderT;
	const uint32_t FrameClock = CG->GetClockCounter() % Frame;
	switch (FrameClock & 0x0F)
	{
		case 0:
		{
			const uint8_t Address = (y << 2) & 0xE0 | (x >> 3) & 0x1F;
			SB->SetDataOnMemAddressBus(Address);
			SB->SetActive(BUS_RAS);
			break;
		}
		case 1:
		{
			const uint8_t Address = 0x40 | (y >> 3) & 0x18 | y & 0x07;
			SB->SetDataOnMemAddressBus(Address);
			SB->SetActive(BUS_CAS);
			SB->SetInactive(BUS_WE);
			break;
		}
		case 2:
		{
			break;
		}
		case 3:
		{
			Pixels = SB->GetDataOnMemDataBus();
			break;
		}
		case 4:
		{
			SB->SetInactive(BUS_CAS);
			break;
		}
		case 5:
		{
			const uint8_t Address = 0x58 | (y >> 6) & 0x03;
			SB->SetDataOnMemAddressBus(Address);
			SB->SetActive(BUS_CAS);
			SB->SetInactive(BUS_WE);
			break;
		}
		case 6:
		{
			break;
		}
		case 7:
		{
			Attribute = SB->GetDataOnMemDataBus();
			break;
		}
		case 8:
		{
			SB->SetInactive(BUS_RAS);
			SB->SetInactive(BUS_CAS);
			bDrawPixels = true;
			break;
		}
		case 9:
		{
			break;
		}
		case 10:
		{
			break;
		}
		case 11:
		{
			break;
		}
		case 12:
		{
			break;
		}
		case 13:
		{
			break;
		}
		case 14:
		{
			break;
		}
		case 15:
		{
			break;
		}
	}
}

void FULA::DrawLogic(uint32_t FrameClock)
{
	if (bDrawPixels)
	{
		//const uint64_t ClockCounter = CG->GetClockCounter();
		//if ((ClockCounter & 0x01))
		//{
		//	return;
		//}
		//const uint32_t FrameClock = (ClockCounter >> FrequencyDivider) % Frame;
		const bool bPixel = !!(Pixels & 0x80);
		const bool bBright = !!(Attribute & 0x40);
		const bool bFlash = !!(Attribute & 0x80);
		const uint8_t Inc = Attribute & 0x07;
		const uint8_t Paper = (Attribute >> 3) & 0x07;
		DisplayData[Y * DisplayWidth + X] = ((bPixel ^ (bFlash && bFlipFlopFlash)) ? Inc : Paper) | (bBright << 3);

		Pixels <<= 1;

		if ((FrameClock % 8) == 3)
		{
			bDrawPixels = false;
		}
	}
}
