#include "Device.h"

FDevice::FDevice(FName Name)
	: DeviceName(Name)
	, bRegistered(false)
{}

void FDevice::Register()
{
	bRegistered = true;
	auto a = DeviceName.ToString();
	std::cout << std::format(TEXT("Debugger: Device {} is registered."), DeviceName.ToString()).c_str() << std::endl;
}

void FDevice::Unregister()
{
	bRegistered = false;
	std::cout << std::format(TEXT("Debugger: Device {} is unregistered."), DeviceName.ToString()).c_str() << std::endl;
}

