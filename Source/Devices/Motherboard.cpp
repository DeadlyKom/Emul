#include "Motherboard.h"
#include "Device.h"

FMotherboard::FMotherboard()
{}

void FMotherboard::Initialize(const std::vector<std::shared_ptr<FDevice>>& InitializationDevices)
{
	for (const std::shared_ptr<FDevice> Device : InitializationDevices)
	{
		auto It = std::find_if(Devices.begin(), Devices.end(),
			[=](const std::shared_ptr<FDevice>& _Device) -> bool
			{
				return _Device ? _Device->GetName() == Device->GetName() : false;
			});

		if (It == Devices.end())
		{
			Device->Register();
			Devices.push_back(Device);
		}
		else
		{
			assert(false);
		}
	}
}

void FMotherboard::Shutdown()
{
	for (const std::shared_ptr<FDevice> Device : Devices)
	{
		if (Device)
		{
			Device->Unregister();
		}
	}
}
