#include "Motherboard_Thread.h"
#include "Devices/Device.h"
#include "AppFramework.h"

#include "Devices/CPU/Z80.h"

FThread::FThread(FName Name)
	: ThreadName(Name)
	, ThreadStatus(EThreadStatus::Stop)
{}

void FThread::Initialize()
{
	CG.SetSampling(2); // tick emulation sampling is divided into two half-cycles
	Thread = std::thread(&FThread::Thread_Execution, this);
}

void FThread::Shutdown()
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[this]() -> void
		{
			Device_Unregistration();
			ThreadRequest_SetStatus(EThreadStatus::Quit);
		});
	Thread.join();
}

void FThread::Reset()
{
	Thread_Request(EThreadTypeRequest::Reset);
}

void FThread::NonmaskableInterrupt()
{
	Thread_Request(EThreadTypeRequest::NonmaskableInterrupt);
}

void FThread::SetFrequency(double Frequency)
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[=, this]() -> void
		{
			CG.SetFrequency(Frequency);
		});
}

void FThread::AddDevices(std::vector<std::shared_ptr<FDevice>> _Devices)
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[=, this]() -> void
		{
			Device_Registration(_Devices);
		});
}

void FThread::Inut_Debugger(bool bEnterDebugger)
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[=, this]() -> void
		{
			ThreadRequest_SetStatus(bEnterDebugger ? EThreadStatus::Stop : EThreadStatus::Run);
		});
}

void FThread::Input_Step(FCPU_StepType::Type Type)
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[=, this]() -> void
		{
			ThreadRequest_Step(Type);
		});
}

void FThread::Device_Registration(const std::vector<std::shared_ptr<FDevice>>& _Devices)
{
	for (const std::shared_ptr<FDevice> Device : _Devices)
	{
		auto It = std::find_if(Devices.begin(), Devices.end(),
			[=](const std::shared_ptr<FDevice>& _Device) -> bool
			{
				return _Device ? _Device->GetName() == Device->GetName() : false;
			});

		if (It == Devices.end())
		{
			Device->InternalRegister(SB, CG);
			Devices.push_back(Device);
		}
		else
		{
			LOG_CONSOLE("[{}] : The device cannot be reinitialized.", ThreadName.ToString());
		}
	}

	// devices are ordering according to processing priorities
	std::sort(Devices.begin(), Devices.end(),
		[](const std::shared_ptr<FDevice>& _DeviceA, const std::shared_ptr<FDevice>& _DeviceB)
		{
			return (_DeviceA && _DeviceB) ? static_cast<uint8_t>(_DeviceA->GetType()) < static_cast<uint8_t>(_DeviceB->GetType()) : false;
		});
}

void FThread::Device_Unregistration()
{
	for (std::shared_ptr<FDevice>& Device : Devices)
	{
		if (Device) Device->InternalUnregister();
	}
}

std::vector<std::shared_ptr<FDevice>> FThread::Device_GetByType(EDeviceType Type)
{
	std::vector<std::shared_ptr<FDevice>> Result;
	for (std::shared_ptr<FDevice>& Device : Devices)
	{
		if (Device && Device->GetType() == Type)
		{
			Result.push_back(Device);
		}
	}
	return std::move(Result);
}

void FThread::Thread_Request(EThreadTypeRequest TypeRequest, Callback&& Task/* = nullptr*/)
{
	ThreadRequest.Push({ TypeRequest, std::move(Task) });
}

std::any FThread::Thread_ThreadRequestResult(EName::Type DeviceID, const std::type_index& Type)
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[=]()
		{
			GetState_RequestHandler(DeviceID, Type);
		});

	return ThreadRequestResult.Pop();
}

void FThread::Thread_Execution()
{
	LOG_CONSOLE("[{}] : Thread started.", ThreadName.ToString());

	// main loop
	while (ThreadStatus != EThreadStatus::Quit)
	{
		while (ThreadStatus == EThreadStatus::Run)
		{
			CG.Tick();		// internal clock generator
			for (std::shared_ptr<FDevice>& Device : Devices)
			{
				if (Device) Device->Tick();
			}

			// ToDo check request at end of frame
			Thread_RequestHandling();
		}

		while (ThreadStatus == EThreadStatus::Trace)
		{
			CG.Tick();		// internal clock generator

			bool bStopTrace = false;
			for (std::shared_ptr<FDevice> Device : Devices)
			{
				if (Device)
				{
					bStopTrace |= Device->TickStopCondition([this](std::shared_ptr<FDevice> _Device) -> bool { return ThreadRequest_StopCondition(_Device); });
				}
			}
			if (bStopTrace) { StepType = FCPU_StepType::None; ThreadRequest_SetStatus(EThreadStatus::Stop); }
		}

		while (ThreadStatus == EThreadStatus::Stop)
		{
			TM.Tick();
			Thread_RequestHandling();
		}
	};

	LOG_CONSOLE("[{}] : Thread shutdown.", ThreadName.ToString());
}

void FThread::Thread_RequestHandling()
{
	if (!ThreadRequest.IsEmpty())
	{
		const auto& [TypeRequest, Task] = ThreadRequest.Pop();
		switch (TypeRequest)
		{
		case EThreadTypeRequest::None:																	break;
		case EThreadTypeRequest::ExecuteTask:			ThreadRequest_ExecuteTask(std::move(Task));		break;
		case EThreadTypeRequest::Reset:					ThreadRequest_Reset();							break;
		case EThreadTypeRequest::NonmaskableInterrupt:	ThreadRequest_NonmaskableInterrupt();			break;
		}
	}
}

void FThread::ThreadRequest_SetStatus(EThreadStatus NewStatus)
{
	ThreadStatus = NewStatus;
}

void FThread::ThreadRequest_Step(FCPU_StepType::Type Type)
{
	if (StepType == Type)
	{
		return;
	}

	StepType = Type;
	ThreadRequest_SetStatus(EThreadStatus::Trace);
}

bool FThread::ThreadRequest_StopCondition(std::shared_ptr<FDevice> Device)
{
	std::shared_ptr<FCPU_Z80> CPU = std::dynamic_pointer_cast<FCPU_Z80>(Device);
	if (CPU == nullptr)
	{
		return false;
	}

	switch (StepType)
	{
	case FCPU_StepType::StepTo:
		break;
	case FCPU_StepType::StepInto:
		break;
	case FCPU_StepType::StepOver:
		break;
	case FCPU_StepType::StepOut:
		break;
	}

	return CPU->Registers.Step == DecoderStep::Completed && CPU->Registers.CP.IsEmpty();
}

void FThread::ThreadRequest_ExecuteTask(const Callback&& Task)
{
	if (Task) Task();
}

void FThread::ThreadRequest_Reset()
{
	CG.Reset();

	for (std::shared_ptr<FDevice>& Device : Devices)
	{
		if (Device) Device->Reset();
	}

	ThreadRequest_SetStatus(EThreadStatus::Run);
	SB.SetActive(BUS_RESET);
	ADD_EVENT(CG, CG.ToSec(0.2f), "End signal RESET",
		[&]()
		{
			SB.SetInactive(BUS_RESET);
		});
}

void FThread::ThreadRequest_NonmaskableInterrupt()
{
	SB.SetActive(BUS_NMI);
	ThreadRequest_SetStatus(EThreadStatus::Run);
	TM.SetTimer(0.2f,
		[&]()
		{
			SB.SetInactive(BUS_NMI);
		}, false, "End signal NMI");
}

void FThread::GetState_RequestHandler(EName::Type DeviceID, const std::type_index& Type)
{
	if (DeviceID == EName::None)
	{
		// get thread status
		if (Type == typeid(EThreadStatus))
		{
			const EThreadStatus Status = ThreadStatus;
			return ThreadRequestResult.Push(Status);
		}
		else if (Type == typeid(uint64_t))
		{
			return ThreadRequestResult.Push(CG.GetClockCounter());
		}
	}
	else
	{
		for (std::shared_ptr<FDevice>& Device : Devices)
		{
			if (Device->UniqueDeviceID != DeviceID)
			{
				continue;
			}

			if (Type == typeid(FRegisters))
			{
				// ToDo need an interface to receive data from the CPU
				if (std::shared_ptr<FCPU_Z80> CPU = std::dynamic_pointer_cast<FCPU_Z80>(Device))
				{
					return ThreadRequestResult.Push(CPU->Registers);
				}
			}
		}
	}
	assert(false);
}
