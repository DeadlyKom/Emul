#include "AccessToROM.h"
#include "Devices/Device.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME() FName(std::format("{}", ThisDeviceName))

namespace
{
	static const char* ThisDeviceName = "Access to ROM";
}

FAccessToROM::FAccessToROM()
	: FDevice(DEVICE_NAME(), EName::AccessToROM, EDeviceType::ControlUnit)
{}

void FAccessToROM::Tick()
{
	const bool bAddress = !!SB->GetSignal(BUS_A14) || !!SB->GetSignal(BUS_A15);
	const bool bControl = !!SB->GetSignal(BUS_RD)  || !!SB->GetSignal(BUS_MREQ);
	const bool bReadROM = bAddress || bControl;

	SB->SetSignal(BUS_RD_ROM, bReadROM ? ESignalState::High : ESignalState::Low);
}
