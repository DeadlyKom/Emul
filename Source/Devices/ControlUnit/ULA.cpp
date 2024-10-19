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
{
	DisplayWidth = DisplayCycles.BorderL + DisplayCycles.DisplayH + DisplayCycles.BorderR;
	DisplayHeight = DisplayCycles.BorderT + DisplayCycles.DisplayV + DisplayCycles.BorderB;

	Scanline = DisplayCycles.FlybackH + DisplayCycles.BorderL + DisplayCycles.DisplayH + DisplayCycles.BorderR;
	Line = DisplayCycles.FlybackV + DisplayCycles.BorderT + DisplayCycles.DisplayV + DisplayCycles.BorderB;
	Frame = Scanline * Line;

	Display.resize(DisplayWidth * DisplayHeight);
}

void FULA::Tick()
{
	bool bIsInterrupt = Frame;
}

void FULA::CalculateFrequency(double MainFrequency)
{
	FrequencyDivider = FMath::CeilLogTwo(FMath::RoundToInt32(MainFrequency / Frequency));
}

void FULA::SetDisplayCycles(const FDisplayCycles& NewDisplayCycles)
{
	DisplayCycles = NewDisplayCycles;
}
