#pragma once

#include <CoreMinimal.h>
#include "Device_define.h"

class FThread;
class FMotherboard;
class FClockGenerator;

enum class EDeviceType
{
	// device types are arranged according to processing priorities
	CPU,
	ULA,
	Memory,
	IO,
};

class FDevice : public std::enable_shared_from_this<FDevice>
{
	friend FThread;
	friend FMotherboard;
public:
	FDevice(FName Name, EDeviceType Type);
	FDevice() = default;

	FName GetName() const { return DeviceName; }
	EDeviceType GetType() const { return DeviceType; }

	// virtual methods
	virtual void Tick(FClockGenerator& CG, uint64_t& InOutISB) {};
	virtual void Reset() {};

protected:
	FName DeviceName;
	EDeviceType DeviceType;

private:
	void Register();
	void Unregister();

	bool bRegistered;
};
