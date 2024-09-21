#pragma once

#include <CoreMinimal.h>

class FDevice;

class FMotherboard
{
public:
	FMotherboard();
	
	void Initialize(const std::vector<std::shared_ptr<FDevice>>& InitializationDevices);
	void Shutdown();

private:
	std::vector<std::shared_ptr<FDevice>> Devices;
};
