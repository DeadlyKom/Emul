#include "AccessToROM.h"

#include "Devices/Device.h"
#include "Utils/Signal/Bus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME() FName(std::format("{}", ThisDeviceName))

namespace
{
	static const char* ThisDeviceName = "Access to ROM";
}

FAccessToROM::FAccessToROM()
	: FDevice(DEVICE_NAME(), NAME_AccessToROM, EDeviceType::ControlUnit)
{}

void FAccessToROM::Tick()
{
	const ESignalState::Type Address	  = SB->GetSignal(BUS_A14) || SB->GetSignal(BUS_A15);
	const ESignalState::Type ReadControl  = SB->GetSignal(BUS_RD)  || SB->GetSignal(BUS_MREQ);
	const ESignalState::Type WriteControl = SB->GetSignal(BUS_WR)  || SB->GetSignal(BUS_MREQ);
	const ESignalState::Type ReadROM  = Address || ReadControl;
	const ESignalState::Type WriteROM = Address || WriteControl;

	SB->SetSignal(BUS_RD_ROM, ReadROM);
	SB->SetSignal(BUS_WR_ROM, WriteROM);
}
