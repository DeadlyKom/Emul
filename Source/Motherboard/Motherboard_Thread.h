#pragma once

#include <CoreMinimal.h>
#include "Utils/Queue.h"
#include "Utils/SignalsBus.h"
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
	Trace,
	Quit
};

enum class EThreadTypeRequest
{
	None,
	ExecuteTask,

	Reset,
	NonmaskableInterrupt,
};

enum class EThreadTypeStateRequest
{
	IsRun,
};

namespace FCPU_StepType { enum Type; }

class FThread
{
	using Callback = std::function<void()>;

	friend FBoard;
	friend FMotherboard;
public:
	FThread(FName Name);
	FThread() = default;
	FThread& operator=(const FThread&) = delete;

	// device setup
	void AddDevices(std::vector<std::shared_ptr<FDevice>> _Devices);

private:
	void Initialize();
	void Shutdown();

	// external signals
	void Reset();
	void NonmaskableInterrupt();

	// setup devices
	void SetFrequency(double Frequency);

	// input
	void Inut_Debugger(bool bEnterDebugger);
	void Input_Step(FCPU_StepType::Type Type);

	void Device_Registration(const std::vector<std::shared_ptr<FDevice>>& _Devices);
	void Device_Unregistration();
	std::vector<std::shared_ptr<FDevice>> Device_GetByType(EDeviceType Type);

	void Thread_Request(EThreadTypeRequest TypeRequest, Callback&& Task = nullptr);
	std::any Thread_ThreadRequestResult(EName::Type DeviceID, const std::type_index& Type);
	void Thread_Execution();
	void Thread_RequestHandling();

	void ThreadRequest_SetStatus(EThreadStatus NewStatus);
	void ThreadRequest_Step(FCPU_StepType::Type Type);
	bool ThreadRequest_StopCondition(std::shared_ptr<FDevice> Device);
	void ThreadRequest_ExecuteTask(const Callback&& Task);
	void ThreadRequest_Reset();
	void ThreadRequest_NonmaskableInterrupt();

	void GetState_RequestHandler(EName::Type DeviceID, const std::type_index& Type);

	template<typename T>
	T GetState(EName::Type DeviceID)
	{
		std::any Result = Thread_ThreadRequestResult(DeviceID, std::type_index(typeid(T)));
		try
		{
			return std::any_cast<T>(Result);
		}
		catch (const std::bad_any_cast& e)
		{
			std::cout << "Error: " << e.what() << std::endl;
			return T();
		}
	}

	FName ThreadName;

	FSignalsBus SB;
	FTimerManager TM;
	FClockGenerator CG;

	FCPU_StepType::Type StepType;

	std::thread Thread;
	std::atomic<EThreadStatus> ThreadStatus;
	TQueue<std::pair<EThreadTypeRequest, Callback>> ThreadRequest;
	TQueue<std::any> ThreadRequestResult;
	std::vector<std::shared_ptr<FDevice>> Devices;
};
