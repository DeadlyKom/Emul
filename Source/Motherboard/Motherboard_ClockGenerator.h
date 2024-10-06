#pragma once

#include <CoreMinimal.h>

#ifdef NDEBUG
#define ADD_EVENT(clock_generator, Rate, DebugName, EventCallback) (clock_generator.AddEvent(Rate, EventCallback))
#define ADD_EVENT_(clock_generator, Rate, DebugName, EventCallback) (clock_generator->AddEvent(Rate, EventCallback))
#else
#define ADD_EVENT(clock_generator, Rate, DebugName, EventCallback) (clock_generator.AddEvent(Rate, EventCallback, DebugName))
#define ADD_EVENT_(clock_generator, Rate, DebugName, EventCallback) (clock_generator->AddEvent(Rate, EventCallback, DebugName))
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

	double GetFrequency() const { return FrequencyInv; }
	void SetSampling(uint32_t _Sampling) { Sampling = _Sampling; }
	void SetFrequency(double _Frequency) { FrequencyInv = 1.0 / (_Frequency * (double)Sampling); }
	FORCEINLINE uint64_t GetClockCounter() const { return ClockCounter; }
	FORCEINLINE void Increment(uint32_t& Counter) const { Counter++; }
	FORCEINLINE uint32_t ToFullCycles(uint32_t Counter) const { return Counter / Sampling; }
	//FORCEINLINE void IncrementByDiscreteness(uint32_t& Counter) const { Counter += Sampling; }

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
