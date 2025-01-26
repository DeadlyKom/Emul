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
	FDevice(FName Name, EName::Type UniqueID, EDeviceType Type, double _Frequency = 0.0f);
	FDevice() = default;
	virtual ~FDevice() = default;

	FORCEINLINE FName GetName() const { return DeviceName; }
	FORCEINLINE EDeviceType GetType() const { return DeviceType; }
	FORCEINLINE FSignalsBus& GetSignalsBus() { return *SB; }
	FORCEINLINE FClockGenerator& GetClockGenerator() { return *CG; }

	// virtual methods
	virtual void Tick() {};
	virtual bool MainTick();
	virtual bool TickStopCondition(std::function<bool(std::shared_ptr<FDevice>)>&& Condition);
	virtual void Reset() {};
	virtual void CalculateFrequency(double MainFrequency, uint32_t Sampling) {};
	virtual std::ostream& Serialize(std::ostream& os) const { return os; }
	virtual std::istream& Deserialize(std::istream& is) { return is; }

protected:
	virtual void Register() {}
	virtual void Unregister() {}

	FName DeviceName;
	EName::Type UniqueDeviceID;
	EDeviceType DeviceType;

	uint32_t FrequencyDivider;
	double Frequency;

	FSignalsBus* SB;
	FClockGenerator* CG;

private:
	void InternalRegister(FSignalsBus& _SB, FClockGenerator& _CG);
	void InternalUnregister();

	uint64_t OldClockCounter;
	bool bRegistered;
};
