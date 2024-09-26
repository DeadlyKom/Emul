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
	virtual void Tick() {};
	virtual void Reset() {};

protected:
	FName DeviceName;
	EDeviceType DeviceType;
	FSignalsBus* SB;
	FClockGenerator* CG;

	virtual void Register() {}
	virtual void Unregister() {}

private:
	void InternalRegister(FSignalsBus& _SB, FClockGenerator& _CG);
	void InternalUnregister();

	bool bRegistered;
};
