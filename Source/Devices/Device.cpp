#include "Device.h"
#include "AppFramework.h"

FDevice::FDevice(FName Name, EDeviceType Type)
	: DeviceName(Name)
	, DeviceType(Type)
	, bRegistered(false)
{}

void FDevice::Register()
{
	bRegistered = true;
	LOG_CONSOLE(("[Device] : {} is registered."), DeviceName.ToString());
}

void FDevice::Unregister()
{
	bRegistered = false;
	LOG_CONSOLE(("[Device] : {} is unregistered."), DeviceName.ToString());
}

