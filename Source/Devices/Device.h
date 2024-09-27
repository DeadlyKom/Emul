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

	FORCEINLINE FName GetName() const { return DeviceName; }
	FORCEINLINE EDeviceType GetType() const { return DeviceType; }
	FORCEINLINE FSignalsBus& GetSignalsBus() { return *SB; }
	FORCEINLINE FClockGenerator& GetClockGenerator() { return *CG; }

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
