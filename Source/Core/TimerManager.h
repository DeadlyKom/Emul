#pragma once

#include <CoreMinimal.h>
#include "SystemTime.h"

enum class ETimerStatus
{
	NotInitialize,
	Active,
	Paused,
	Executing,
};

struct FTimerHandle
{
	friend class FTimerManager;

	FTimerHandle();

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
	int32_t Handle;
};

struct FTimerData
{
	bool bLoop = false;
	int64_t Rate = -1;
	int64_t ExpireTime = -1;
	ETimerStatus Status = ETimerStatus::NotInitialize;
	FTimerHandle Handle;
	std::string DebugName;
	std::function<void(void)> Callback;
};

// ToDo: finish it
class FTimerManager
{
public:
	FTimerManager();

	float Tick();
	void SetTimer(float InRate, std::function<void(void)>&& Callback, bool bLoop = false, const std::string& _DebugName = "");
	void SetTimer(FTimerHandle& InOutHandle, std::function<void(void)>&& Callback, float InRate, bool bLoop = false, const std::string& _DebugName = "");
	void ClearTimer(FTimerHandle Handle);
	void PauseTimer(FTimerHandle Handle);
	void UnPauseTimer(FTimerHandle Handle);

	float GetTimerRate(FTimerHandle Handle) const;
	FTimerData& GetTimer(FTimerHandle Handle);

protected:
	FTimerData* FindTimer(FTimerHandle Handle);
	void RemoveTimer(FTimerHandle Handle);

private:
	FSystemTime Time;
	std::vector<FTimerData> Timers;
};
