#include "SystemTime.h"

double FSystemTime::CpuTickDelta = 0.0;

void FSystemTime::Initialize()
{
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);
	CpuTickDelta = 1.0 / static_cast<double>(Frequency.QuadPart);
}

int64_t FSystemTime::GetCurrentTick()
{
	LARGE_INTEGER CurrentTick;
	QueryPerformanceCounter(&CurrentTick);
	return static_cast<int64_t>(CurrentTick.QuadPart);
}

void FSystemTime::BusyLoopSleep(float SleepTime)
{
	int64_t FinalTick = (int64_t)((double)SleepTime / CpuTickDelta) + GetCurrentTick();
	while (GetCurrentTick() < FinalTick);
}


