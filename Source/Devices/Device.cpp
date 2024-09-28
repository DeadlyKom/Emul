#include "Device.h"
#include "AppFramework.h"

FDevice::FDevice(FName Name, EName::Type DeviceID, EDeviceType Type)
	: DeviceName(Name)
	, UniqueDeviceID(DeviceID)
	, DeviceType(Type)
	, bRegistered(false)
	, SB(nullptr)
	, CG(nullptr)
{}

bool FDevice::TickStopCondition(std::function<bool(std::shared_ptr<FDevice>)>&& Condition)
{
	Tick();
	return Condition(shared_from_this());
}

void FDevice::InternalRegister(FSignalsBus& _SB, FClockGenerator& _CG)
{
	bRegistered = true;
	CG = &_CG;
	SB = &_SB;
	LOG_CONSOLE(("[Device] : {} is registered."), DeviceName.ToString());

	Register();
}

void FDevice::InternalUnregister()
{
	bRegistered = false;
	LOG_CONSOLE(("[Device] : {} is unregistered."), DeviceName.ToString());

	Unregister();
}

