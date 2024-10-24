#pragma once

#include <CoreMinimal.h>

#ifdef NDEBUG
#define ADD_EVENT(clock_generator, Rate, FrequencyDivider, EventCallback, DebugName) (clock_generator.AddEvent((uint64_t)Rate << FrequencyDivider, EventCallback))
#define ADD_EVENT_(clock_generator, Rate, FrequencyDivider, EventCallback, DebugName) (clock_generator->AddEvent((uint64_t)Rate << FrequencyDivider, EventCallback))
#else
#define ADD_EVENT(clock_generator, Rate, FrequencyDivider, EventCallback, DebugName) (clock_generator.AddEvent((uint64_t)Rate << FrequencyDivider, EventCallback, DebugName))
#define ADD_EVENT_(clock_generator, Rate, FrequencyDivider, EventCallback, DebugName) (clock_generator->AddEvent((uint64_t)Rate << FrequencyDivider, EventCallback, DebugName))
#endif

struct FEventData
{
	uint64_t ExpireTime;
#ifndef NDEBUG
	std::string DebugName;
#endif 
	std::function<void()> Callback;
};

class FClockGenerator
{
public:
	FClockGenerator();
	~FClockGenerator();

	void Tick();
	void Reset();

	uint32_t GetSampling() const { return Sampling; }
	void SetSampling(uint32_t _Sampling) { Sampling = _Sampling; }
	double GetFrequency() const { return FrequencyInv; }
	void SetFrequency(double _Frequency) { FrequencyInv = 1.0 / (_Frequency * (double)Sampling); }

	FORCEINLINE uint64_t GetClockCounter() const { return ClockCounter; }

#ifndef NDEBUG
	void AddEvent(uint64_t Rate, std::function<void()>&& EventCallback, const std::string& _DebugName = "");
#else
	void AddEvent(uint64_t Rate, std::function<void()>&& EventCallback);
#endif 

	FORCEINLINE uint64_t ToSec(double Time) const
	{
		return uint64_t(Time / FrequencyInv);
	}
	FORCEINLINE uint64_t ToNanosec(double Time) const
	{
		return uint64_t((Time * 0.000000001) / FrequencyInv);
	}

private:
	uint32_t Sampling;
	double FrequencyInv;
	uint64_t ClockCounter;

	size_t ElementCount;
	size_t LastElementIndex;
	std::vector<FEventData> Events;
};
