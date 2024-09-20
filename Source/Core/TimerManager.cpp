#include "TimerManager.h"

FTimerManager::FTimerManager()
{
	Time.Initialize();
}

float FTimerManager::Tick()
{
	static int64_t OldTime = Time.GetCurrentTick();

	const int64_t CurrentTime = Time.GetCurrentTick();
	const float DeltaTime = (float)Time.TimeBetweenTicks(OldTime, CurrentTime);
	OldTime = CurrentTime;

	for (FTimerData& Timer : Timers)
	{
		if (!Timer.Handle.IsValid())
		{
			continue;
		}

		if (CurrentTime - Timer.ExpireTime > Timer.Rate)
		{
			auto a = (float)Time.TimeBetweenTicks(Timer.ExpireTime, CurrentTime);
			Timer.Callback();

			if (Timer.bLoop)
			{
				Timer.ExpireTime = CurrentTime;
			}
			else
			{
				RemoveTimer(Timer.Handle);
			}
		}
	}

	return DeltaTime;
}

void FTimerManager::SetTimer(FTimerHandle& InOutHandle, std::function<void(void)>&& Callback, float InRate, bool bLoop)
{
	if (FindTimer(InOutHandle))
	{
		RemoveTimer(InOutHandle);
	}

	if (InRate > 0.f)
	{
		FTimerData NewTimerData
		{
			.bLoop = bLoop,
			.Rate = FSystemTime::SecondsToTicks(InRate),
			.ExpireTime = Time.GetCurrentTick(),
			.Status = ETimerStatus::Active,
			.Handle = (int32_t)Timers.size(),
			.Callback = std::move(Callback),
		};
		InOutHandle = NewTimerData.Handle;
		Timers.push_back(NewTimerData);
	}
	else
	{
		InOutHandle.Invalidate();
	}
}

FTimerData& FTimerManager::GetTimer(FTimerHandle const& InHandle)
{
	int32_t Index = InHandle.GetIndex();
	assert(Index >= 0 && Index < Timers.size() && Timers[Index].Handle == InHandle);
	FTimerData& Timer = Timers[Index];
	return Timer;
}

FTimerData* FTimerManager::FindTimer(const FTimerHandle& InHandle)
{
	if (!InHandle.IsValid())
	{
		return nullptr;
	}

	int32_t Index = InHandle.GetIndex();
	if (Index < 0 || Index >= Timers.size())
	{
		return nullptr;
	}

	FTimerData& Timer = Timers[Index];
	if (Timer.Handle != InHandle)
	{
		return nullptr;
	}

	return &Timer;
}

void FTimerManager::RemoveTimer(FTimerHandle Handle)
{
	FTimerData& TimerData = Timers[Handle.GetIndex()];
	TimerData.Callback = nullptr;
	TimerData.Handle.Invalidate();
	TimerData.Status = ETimerStatus::Removed;
}