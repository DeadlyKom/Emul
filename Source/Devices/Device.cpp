#include "Device.h"
#include "AppFramework.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

FDevice::FDevice(FName Name, EName::Type DeviceID, EDeviceType Type, double _Frequency /*= 0.0f*/)
	: DeviceName(Name)
	, UniqueDeviceID(DeviceID)
	, DeviceType(Type)
	, SB(nullptr)
	, CG(nullptr)
	, bRegistered(false)
	, FrequencyDivider(0)
	, Frequency(_Frequency)
{}

bool FDevice::MainTick()
{
	if (FrequencyDivider != 0)
	{
		const uint64_t RealCounter = CG->GetClockCounter() >> FrequencyDivider;
		if (RealCounter == OldClockCounter)
		{
			return false;
		}
		OldClockCounter = RealCounter;
	}
	Tick();
	return true;
}

bool FDevice::TickStopCondition(std::function<bool(std::shared_ptr<FDevice>)>&& Condition)
{
	if (!MainTick())
	{
		return false;
	}
	return Condition(shared_from_this());
}

void FDevice::InternalRegister(FSignalsBus& _SB, FClockGenerator& _CG)
{
	bRegistered = true;
	CG = &_CG;
	SB = &_SB;
	LOG(("[Device] : {} is registered."), DeviceName.ToString());

	Register();
}

void FDevice::InternalUnregister()
{
	bRegistered = false;
	LOG(("[Device] : {} is unregistered."), DeviceName.ToString());

	Unregister();
}

