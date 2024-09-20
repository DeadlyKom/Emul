#pragma once

#include <CoreMinimal.h>
#include "SystemTime.h"

enum class ETimerStatus
{
	Active,
	Paused,
	Executing,
	Removed
};

struct FTimerHandle
{
	friend class FTimerManager;

	FTimerHandle(int32_t _Handle = INDEX_NONE)
		: Handle(_Handle)
	{}

	bool IsValid() const
	{
		return Handle != INDEX_NONE;
	}

	void Invalidate()
	{
		Handle = INDEX_NONE;
	}

	bool operator==(const FTimerHandle& Other) const
	{
		return Handle == Other.Handle;
	}

	bool operator!=(const FTimerHandle& Other) const
	{
		return Handle != Other.Handle;
	}

private:
	int32_t GetIndex() const
	{
		return Handle;
	}

	int32_t Handle;
};

struct FTimerData
{
	bool bLoop;
	int64_t Rate;
	int64_t ExpireTime;
	ETimerStatus Status;
	FTimerHandle Handle;
	std::function<void(void)> Callback;
};

// ToDo: finish it
class FTimerManager
{
public:
	FTimerManager();

	float Tick();
	void SetTimer(FTimerHandle& InOutHandle, std::function<void(void)>&& Callback, float InRate, bool bLoop);
	void ClearTimer(FTimerHandle& InHandle);
	void PauseTimer(FTimerHandle InHandle);
	void UnPauseTimer(FTimerHandle InHandle);

	float GetTimerRate(FTimerHandle InHandle) const;
	FTimerData& GetTimer(FTimerHandle const& InHandle);

	FSystemTime Time;

protected:
	FTimerData* FindTimer(const FTimerHandle& InHandle);
	void RemoveTimer(FTimerHandle Handle);

private:
	std::vector<FTimerData> Timers;
};
