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
	, bIsBorder(false)
	, bBorderDelay(false)
	, bIsVideoFetch(false)
	, bIsFlyback(false)
	, bDrawPixels(false)
	, bFlipFlopFlash(false)
	, bIsInterrupt(false)
	, bInterruptLatch(false)
	, X(0)
	, Y(0)
	, FlashCounter(32)
	, Pixels(0)
	, Attribute(0)
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
	if (bIsFirst) ULALogic(FrameClock);

	if ((bIsBorder || bBorderDelay) && (!bIsVideoFetch || bBorderDelay))
	{
		// ToDo read port #FE
		const uint8_t Color = 15;
		DisplayData[Y * DisplayWidth + X] = Color;
	}
	
	if (bIsVideoFetch)
	{
		VideoFetch();
	}

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
			if (bInterruptLatch)
			{
				if (!--FlashCounter)
				{
					FlashCounter = 32;
					bFlipFlopFlash = !bFlipFlopFlash;
				}
			}
		}
		bInterruptLatch = bIsInterrupt;
	}
}

void FULA::ULALogic(uint32_t FrameClock)
{
	const bool bIsFlybackH = FrameClock < DisplayCycles.FlybackH;
	const bool bIsFlybackV = FrameClock < DisplayCycles.FlybackV * Scanline;
	bIsFlyback = bIsFlybackH || bIsFlybackV;

	if (bIsFlybackV)
	{
		Y = 0;
		X = 0;
		bIsBorder = false;
		bIsVideoFetch = false;
	}
	else
	{
		const uint32_t LocalFrameClock = FrameClock - DisplayCycles.FlybackV * Scanline;
		Y = LocalFrameClock / Scanline;

		const uint32_t LocalLineClock = LocalFrameClock - Y * Scanline;
		X = LocalLineClock > DisplayCycles.FlybackH ? (LocalLineClock - DisplayCycles.FlybackH) % DisplayWidth : 0;

		const bool bIsBorderH = X < DisplayCycles.BorderL || X >= (DisplayCycles.BorderL + DisplayCycles.DisplayH);
		const bool bIsBorderV = Y < DisplayCycles.BorderT || Y >= (DisplayCycles.BorderT + DisplayCycles.DisplayV);
		bIsBorder = bIsBorderH || bIsBorderV;

		bIsVideoFetch = !bIsBorderV && (X >= DisplayCycles.BorderL && X < (DisplayCycles.BorderL + DisplayCycles.DisplayH + 16));
		bBorderDelay = bIsVideoFetch && (X < (DisplayCycles.BorderL + 16));
	}
}

void FULA::VideoFetch()
{
	const int32_t x = X - (DisplayCycles.BorderL + 8);
	const int32_t y = Y - DisplayCycles.BorderT;
	const uint32_t FrameClock = CG->GetClockCounter() % Frame;
	switch (FrameClock & 0x1F)
	{
		case 0:
		{
			PixelsShift = Pixels >> 8;
			Pixels <<= 8;
			AttributeLatch = Attribute >> 8;
			Attribute <<= 8;
			break;
		}
		case 16:
		{
			PixelsShift = Pixels >> 8;
			Pixels <<= 8;
			AttributeLatch = Attribute >> 8;
			Attribute <<= 8;

			const uint8_t Address = (y << 2) & 0xE0 | (x >> 3) & 0x1F | 0;
			SB->SetDataOnMemAddressBus(Address);
			SB->SetActive(BUS_RAS);
			break;
		}
		case 17:
		{
			const uint8_t Address = 0x40 | (y >> 3) & 0x18 | y & 0x07;
			SB->SetDataOnMemAddressBus(Address);
			SB->SetActive(BUS_CAS);
			SB->SetInactive(BUS_WE);
			break;
		}
		//case 18:
		//{
		//	break;
		//}
		case 19:
		{
			Pixels |= SB->GetDataOnMemDataBus() << 8;
			SB->SetInactive(BUS_CAS);
			break;
		}
		case 20:
		{
			const uint8_t Address = 0x58 | (y >> 6) & 0x03;
			SB->SetDataOnMemAddressBus(Address);
			SB->SetActive(BUS_CAS);
			SB->SetInactive(BUS_WE);
			break;
		}
		//case 21:
		//{
		//	break;
		//}
		case 22:
		{
			Attribute |= SB->GetDataOnMemDataBus() << 8;
			SB->SetInactive(BUS_CAS);
			break;
		}
		case 23:
		{
			SB->SetInactive(BUS_RAS);
			break;
		}
		case 24:
		{
			const uint8_t Address = (y << 2) & 0xE0 | (x >> 3) & 0x1F | 1;
			SB->SetDataOnMemAddressBus(Address);
			SB->SetActive(BUS_RAS);
			break;
		}
		case 25:
		{
			const uint8_t Address = 0x40 | (y >> 3) & 0x18 | y & 0x07;
			SB->SetDataOnMemAddressBus(Address);
			SB->SetActive(BUS_CAS);
			SB->SetInactive(BUS_WE);
			break;
		}
		//case 26:
		//{
		//	break;
		//}
		case 27:
		{
			Pixels |= SB->GetDataOnMemDataBus();
			SB->SetInactive(BUS_CAS);
			break;
		}
		case 28:
		{
			const uint8_t Address = 0x58 | (y >> 6) & 0x03;
			SB->SetDataOnMemAddressBus(Address);
			SB->SetActive(BUS_CAS);
			SB->SetInactive(BUS_WE);
			break;
		}
		//case 29:
		//{
		//	break;
		//}
		case 30:
		{
			Attribute |= SB->GetDataOnMemDataBus();
			SB->SetInactive(BUS_CAS);
			break;
		}
		case 31:
		{
			SB->SetInactive(BUS_RAS);
			bDrawPixels = true;
			break;
		}
	}
}

void FULA::DrawLogic(uint32_t FrameClock)
{
	if (bDrawPixels)
	{
		const bool bPixel = !!(PixelsShift & 0x80);
		const bool bBright = !!(AttributeLatch & 0x40);
		const bool bFlash = !!(AttributeLatch & 0x80);
		const uint8_t Inc = (AttributeLatch >> 0) & 0x07;
		const uint8_t Paper = (AttributeLatch >> 3) & 0x07;
		DisplayData[Y * DisplayWidth + X] = ((bPixel ^ (bFlash && bFlipFlopFlash)) ? Inc : Paper) | (bBright << 3);
		PixelsShift <<= 1;

		//if ((FrameClock & 0x0F) == 7)
		//{
		//	bDrawPixels = false;
		//}
	}
}
