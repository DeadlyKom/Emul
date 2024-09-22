#pragma once

#include <CoreMinimal.h>

struct FEventData
{
	uint64_t ExpireTime;
	std::string DebugName;
	std::function<void()> Callback;
};

class FClockGenerator
{
public:
	FClockGenerator();

	void SetFrequency(double _Frequency);
	void Tick();
	void Reset();
	uint64_t GetClockCounter() const { return ClockCounter; }

	void AddEvent(uint64_t Rate, std::function<void()>&& Event, const std::string& _DebugName = "");

	uint64_t ToSec(long double Time) const 
	{
		return uint64_t(Time * FrequencyInv);
	}

private:
	double FrequencyInv;
	uint64_t ClockCounter;
	std::vector<FEventData> Events;
};
