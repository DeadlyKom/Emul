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

enum class FCPU_StepType
{
	None,
	StepTo,
	StepInto,
	StepOver,
	StepOut,
};

enum class EThreadStatus
{
	Unknown,
	Stop,
	Run,
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
	void Input_Step(FCPU_StepType Type);

	void Device_Registration(const std::vector<std::shared_ptr<FDevice>>& _Devices);
	void Device_Unregistration();
	std::vector<std::shared_ptr<FDevice>> Device_GetByType(EDeviceType Type);

	void Thread_Request(EThreadTypeRequest TypeRequest, Callback&& Task = nullptr);
	void Device_ThreadRequest(EName::Type DeviceID, const std::type_index& Type, std::any Value);
	std::any Device_ThreadRequestResult(EName::Type DeviceID, const std::type_index& Type);
	void Thread_Execution();
	void Thread_RequestHandling();

	void ThreadRequest_SetStatus(EThreadStatus NewStatus);
	void ThreadRequest_Step(FCPU_StepType Type);
	bool ThreadRequest_StopCondition(std::shared_ptr<FDevice> Device);
	void ThreadRequest_ExecuteTask(const Callback&& Task);
	void ThreadRequest_Reset();
	void ThreadRequest_NonmaskableInterrupt();
	void ThreadRequest_LoadRawData(EName::Type DeviceID, std::filesystem::path FilePath);

	void GetState_RequestHandler(EName::Type DeviceID, const std::type_index& Type);
	void SetState_RequestHandler(EName::Type DeviceID, const std::type_index& Type, const std::any& Value);

	void LoadRawData(EName::Type DeviceID, std::filesystem::path FilePath);
	void Serialize(std::string& Output);
	void Deserialize(const std::string& Input);

	template<typename T>
	T GetState(EName::Type DeviceID)
	{
		std::any Result = Device_ThreadRequestResult(DeviceID, std::type_index(typeid(T)));
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
	
	template<typename T>
	void SetState(EName::Type DeviceID, const std::any& Value)
	{
		Device_ThreadRequest(DeviceID, std::type_index(typeid(T)), Value);
	}

	template<typename T>
	T* GetDevice()
	{
		for (std::shared_ptr<FDevice>& Device : Devices)
		{
			if (std::shared_ptr<T> FoundDevice = std::dynamic_pointer_cast<T>(Device))
			{
				return FoundDevice.get();
			}
		}
		return nullptr;
	}

	template<typename T>
	void SetToContainer(const T& Value)
	{
		Container[std::type_index(typeid(T))] = Value;
	}

	template<typename T>
	T GetFromContainer(const std::type_index& Type)
	{
		auto It = Container.find(Type);
		if (It != Container.end())
		{
			try
			{
				return std::any_cast<T>(It->second);
			}
			catch (const std::bad_any_cast& e)
			{
				std::cout << "Error: " << e.what() << std::endl;
			}	
		}
		return T();
	}

	FName ThreadName;
	bool bInterruptLatch;

	FSignalsBus SB;
	FTimerManager TM;
	FClockGenerator CG;
	std::unordered_map<std::type_index, std::any> Container;

	FCPU_StepType StepType;
	std::string SerializedData;

	std::thread Thread;
	std::atomic<EThreadStatus> ThreadStatus;
	TQueue<std::pair<EThreadTypeRequest, Callback>> ThreadRequest;
	TQueue<std::any> ThreadRequestResult;
	std::vector<std::shared_ptr<FDevice>> Devices;
};
