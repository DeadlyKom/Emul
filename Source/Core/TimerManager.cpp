#include "TimerManager.h"

static int32_t TimerHandleCounter = 0;
static FTimerData DefaultTimerData;

FTimerHandle::FTimerHandle()
	: Handle(TimerHandleCounter++)
{}

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

	Timers.erase(std::remove_if(Timers.begin(), Timers.end(),
		[=](FTimerData& Timer)
		{
			if (!Timer.Handle.IsValid())
			{
				return false;
			}

			if (CurrentTime - Timer.ExpireTime > Timer.Rate)
			{
				Timer.Callback();

				if (Timer.bLoop)
				{
					Timer.ExpireTime = CurrentTime;
					return false;
				}
				return true;
			}

			return false;
		}), Timers.end());

	return DeltaTime;
}

void FTimerManager::SetTimer(float InRate, std::function<void(void)>&& Callback, bool bLoop, const std::string& _DebugName)
{
	FTimerHandle Handle;
	SetTimer(Handle, std::move(Callback), InRate, bLoop, _DebugName);
}

void FTimerManager::SetTimer(FTimerHandle& InOutHandle, std::function<void(void)>&& Callback, float InRate, bool bLoop /*= false*/, const std::string& _DebugName /*= ""*/)
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
			.Handle = FTimerHandle(),
			.DebugName = _DebugName,
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

FTimerData& FTimerManager::GetTimer(FTimerHandle Handle)
{
	FTimerData* Timer = FindTimer(Handle);
	return Timer ? *Timer : DefaultTimerData;
}

FTimerData* FTimerManager::FindTimer(FTimerHandle Handle)
{
	auto It = std::find_if(Timers.begin(), Timers.end(),
		[=](const FTimerData& Timer) -> bool
		{
			return Timer.Handle == Handle;
		});

	return It != Timers.end() ? &(*It) : nullptr;
}

void FTimerManager::RemoveTimer(FTimerHandle Handle)
{
	Timers.erase(std::remove_if(Timers.begin(), Timers.end(),
		[=](const FTimerData& Timer) -> bool
		{
			if (!Timer.Handle.IsValid())
			{
				return false;
			}

			return Timer.Handle == Handle;
		}), Timers.end());
}
