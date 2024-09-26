#include "Device.h"
#include "AppFramework.h"

FDevice::FDevice(FName Name, EDeviceType Type)
	: DeviceName(Name)
	, DeviceType(Type)
	, bRegistered(false)
	, SB(nullptr)
	, CG(nullptr)
{}

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

