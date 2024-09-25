#include "Device.h"
#include "AppFramework.h"

FDevice::FDevice(FName Name, EDeviceType Type)
	: DeviceName(Name)
	, DeviceType(Type)
	, bRegistered(false)
{}

void FDevice::Register(FSignalsBus& SB)
{
	bRegistered = true;
	LOG_CONSOLE(("[Device] : {} is registered."), DeviceName.ToString());
}

void FDevice::Unregister(FSignalsBus& SB)
{
	bRegistered = false;
	LOG_CONSOLE(("[Device] : {} is unregistered."), DeviceName.ToString());
}

