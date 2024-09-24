#pragma once

#include <CoreMinimal.h>
#include "Utils/Queue.h"
#include "Core/TimerManager.h"
#include "Motherboard_ClockGenerator.h"

class FDevice;
class FBoard;
class FMotherboard;

enum class EDeviceType;

enum class EThreadStatus
{
	Run,
	Stop,
	Quit
};

enum class EThreadTypeRequest
{
	None,
	Run,
	Stop,
	Quit,
	ExecuteTask,

	Reset,
	NonmaskableInterrupt,
};

class FThread
{
	friend FBoard;
	friend FMotherboard;
public:
	FThread(FName Name);
	FThread() = default;
	FThread& operator=(const FThread&) = delete;

	// device setup
	void AddDevices(const std::vector<std::shared_ptr<FDevice>>& _Devices);

private:
	void Initialize();
	void Shutdown();

	// external signals
	void Reset();
	void NonmaskableInterrupt();

	// setup devices
	void SetFrequency(double Frequency);

	void Device_Registration(const std::vector<std::shared_ptr<FDevice>>& _Devices);
	void Device_Unregistration();
	std::vector<std::shared_ptr<FDevice>> Device_GetByType(EDeviceType Type);

	void Thread_Request(EThreadTypeRequest TypeRequest, std::function<void()>&& Task = nullptr);
	void Thread_Execution();
	void Thread_RequestHandling();

	void ThreadRequest_SetStatus(EThreadStatus NewStatus);
	void ThreadRequest_ExecuteTask(const std::function<void()>&& Task);
	void ThreadRequest_Reset();
	void ThreadRequest_NonmaskableInterrupt();

	FName ThreadName;

	uint64_t ISB;	// internal signals bus
	FTimerManager TM;
	FClockGenerator CG;

	std::thread Thread;
	std::atomic<EThreadStatus> ThreadStatus;
	TQueue<std::pair<EThreadTypeRequest, std::function<void()>>> ThreadRequest;
	TQueue<std::function<void()>> ThreadRequestResult;
	std::vector<std::shared_ptr<FDevice>> Devices;
};
