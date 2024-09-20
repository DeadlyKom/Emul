#pragma once

#include <CoreMinimal.h>

class FSystemTime
{
public:
	static void Initialize();
	static int64_t GetCurrentTick();
	static void BusyLoopSleep(float SleepTime);
	static inline double TicksToSeconds(int64_t TickCount)
	{
		return TickCount * CpuTickDelta;
	}
	static inline int64_t SecondsToTicks(float Time)
	{
		return (int64_t)std::floor(Time / CpuTickDelta);
	}
	static inline double TicksToMillisecs(int64_t TickCount)
	{
		return TickCount * CpuTickDelta * 1000.0;
	}
	static inline double TimeBetweenTicks(int64_t TickA, int64_t TickB)
	{
		return TicksToSeconds(TickB - TickA);
	}

private:
	static double CpuTickDelta;
};
