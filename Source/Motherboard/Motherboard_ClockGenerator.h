#pragma once

#include <CoreMinimal.h>

enum class ExecSpeed : int { _20PERCENT = 0, HALF, NORMAL, X2, MAX };

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

	void Tick();
	void Reset();

	void SetSampling(uint32_t _Sampling) { Sampling = _Sampling; }
	void SetFrequency(double _Frequency) { FrequencyInv = 1.0 / (_Frequency * (double)Sampling); }
	inline uint64_t GetClockCounter() const { return ClockCounter; }
	inline uint32_t ToFullCycles(uint32_t Counter) const { return Counter / Sampling; }
	inline void Increment(uint32_t& Counter) const { Counter++; }
	inline void IncrementByDiscreteness(uint32_t& Counter) const { Counter += Sampling; }

	void AddEvent(uint64_t Rate, std::function<void()>&& Event, const std::string& _DebugName = "");

	inline uint64_t ToSec(double Time) const 
	{
		return uint64_t(Time * FrequencyInv);
	}
	inline uint64_t ToNanosec(double Time) const
	{
		return uint64_t(Time * FrequencyInv);
	}

private:
	uint32_t Sampling;
	double FrequencyInv;
	uint64_t ClockCounter;
	std::vector<FEventData> Events;
};
