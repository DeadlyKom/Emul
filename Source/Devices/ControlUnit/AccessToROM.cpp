#include "AccessToROM.h"
#include "Devices/Device.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME() FName(std::format("{}", ThisDeviceName))

namespace
{
	static const char* ThisDeviceName = "Access to ROM";
}

const FName FAccessToROM::SignalName_RD_ROM = "Read ROM";

FAccessToROM::FAccessToROM()
	: FDevice(DEVICE_NAME(), EDeviceType::ControlUnit)
{}

void FAccessToROM::Tick(FClockGenerator& CG, FSignalsBus& SB)
{
	const bool bAddress = SB.GetSignal(BUS_A14) || SB.GetSignal(BUS_A15);
	const bool bControl = SB.GetSignal(BUS_RD)  || SB.GetSignal(BUS_MREQ);
	const bool bReadROM = bAddress || bControl;

	if (!bReadROM)
	{
		int a = 10;
	}

	CG.AddEvent(1,
		[=, &SB]()
		{
			SB.SetSignal(SignalName_RD_ROM, bReadROM ? ESignalState::High : ESignalState::Low);
		}, "Delay signal");
	//SB.SetSignal(SignalName_RD_ROM, bReadROM ? ESignalState::High : ESignalState::Low);
}

void FAccessToROM::Register(FSignalsBus& SB)
{
	SB.AddSignal(SignalName_RD_ROM);
}
