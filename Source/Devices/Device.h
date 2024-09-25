#pragma once

#include <CoreMinimal.h>

class FThread;
class FSignalsBus;
class FMotherboard;
class FClockGenerator;

enum class EDeviceType
{
	// device types are arranged according to processing priorities
	CPU,
	ControlUnit,
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
	virtual void Tick(FClockGenerator& CG, FSignalsBus& SB) {};
	virtual void Reset() {};

protected:
	FName DeviceName;
	EDeviceType DeviceType;

private:
	virtual void Register(FSignalsBus& SB);
	virtual void Unregister(FSignalsBus& SB);

	bool bRegistered;
};
