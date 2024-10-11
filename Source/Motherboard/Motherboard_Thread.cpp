#include "Motherboard_Thread.h"
#include "Devices/Device.h"
#include "AppFramework.h"

#include "Devices/CPU/Z80.h"
#include "Devices/CPU/Interface_CPU_Z80.h"
#include "Devices/Memory/Interface_Memory.h"

FThread::FThread(FName Name)
	: ThreadName(Name)
	, StepType(FCPU_StepType::None)
	, ThreadStatus(EThreadStatus::Unknown)
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

void FThread::Input_Step(FCPU_StepType Type)
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

void FThread::Device_ThreadRequest(EName::Type DeviceID, const std::type_index& Type, std::any Value)
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[=]()
		{
			SetState_RequestHandler(DeviceID, Type, Value);
		});
}

std::any FThread::Device_ThreadRequestResult(EName::Type DeviceID, const std::type_index& Type)
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
		while (ThreadStatus <= EThreadStatus::Stop)
		{
			TM.Tick();
			Thread_RequestHandling();
		}

		while (ThreadStatus == EThreadStatus::Run || 
			ThreadStatus == EThreadStatus::Stop && !SB.IsPositiveEdge(ESignalBus::M1))
		{
			CG.Tick();		// internal clock generator
			for (std::shared_ptr<FDevice>& Device : Devices)
			{
				if (Device) Device->Tick();
			}

			// ToDo check request at end of frame
			Thread_RequestHandling();
		}

		//// finish machine cycle
		//if (ThreadStatus == EThreadStatus::Stop)
		//{
		//	/*static auto CheckLambda = [this]() -> bool
		//		{
		//			for (std::shared_ptr<FDevice> Device : Devices)
		//			{
		//				std::shared_ptr<FCPU_Z80> CPU = std::dynamic_pointer_cast<FCPU_Z80>(Device);
		//				if (CPU == nullptr)
		//				{
		//					continue;
		//				}
		//				return SB.IsPositiveEdge(ESignalBus::M1);
		//			}
		//			return true;
		//		};*/
		//	while (!SB.IsPositiveEdge(ESignalBus::M1))
		//	{
		//		CG.Tick();		// internal clock generator
		//		for (std::shared_ptr<FDevice>& Device : Devices)
		//		{
		//			if (Device) Device->Tick();
		//		}
		//	}
		//}

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

void FThread::ThreadRequest_Step(FCPU_StepType Type)
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
	ADD_EVENT(CG, 7, "End signal RESET",
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

void FThread::ThreadRequest_LoadRawData(EName::Type DeviceID, std::filesystem::path FilePath)
{
	for (std::shared_ptr<FDevice>& Device : Devices)
	{
		if (Device->UniqueDeviceID != DeviceID)
		{
			continue;
		}

		if (std::shared_ptr<IMemory> Memory = std::dynamic_pointer_cast<IMemory>(Device))
		{
			return Memory->Load(FilePath);
		}
	}
}

void FThread::GetState_RequestHandler(EName::Type DeviceID, const std::type_index& Type)
{
	switch (DeviceID)
	{
		case EName::None:
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
			else if (Type == typeid(double))
			{
				return ThreadRequestResult.Push(CG.GetFrequency());
			}
			break;
		}
		case EName::Memory:
		{
			FMemorySnapshot MS(CG.GetClockCounter());
			for (std::shared_ptr<FDevice>& Device : Devices)
			{
				if (Device->GetType() == EDeviceType::Memory)
				{
					if (std::shared_ptr<IMemory> Memory = std::dynamic_pointer_cast<IMemory>(Device))
					{
						Memory->Snapshot(MS, EMemoryOperationType::Read);
					}
				}
			}
			return ThreadRequestResult.Push(MS);
		}
		default:
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
					if (std::shared_ptr<ICPU_Z80> CPU = std::dynamic_pointer_cast<ICPU_Z80>(Device))
					{
						return ThreadRequestResult.Push(CPU->GetRegisters());
					}
				}
			}
			break;
		}
	}
	assert(false);
}

void FThread::SetState_RequestHandler(EName::Type DeviceID, const std::type_index& Type, const std::any& Value)
{
	switch (DeviceID)
	{
		case EName::None:
		{
			break;
		}
		case EName::Memory:
		{
			if (Type == typeid(FMemorySnapshot))
			{
				FMemorySnapshot MS;
				try
				{
					MS = std::any_cast<FMemorySnapshot>(Value);
				}
				catch (const std::bad_any_cast& e)
				{
					std::cout << "Error: " << e.what() << std::endl;
					break;
				}

				for (std::shared_ptr<FDevice>& Device : Devices)
				{
					if (Device->GetType() == EDeviceType::Memory)
					{
						if (std::shared_ptr<IMemory> Memory = std::dynamic_pointer_cast<IMemory>(Device))
						{
							Memory->Snapshot(MS, EMemoryOperationType::Write);
						}
					}
				}
				return;
			}
			break;
		}
		default:
		{
			break;
		}
	}
	assert(false);
}

void FThread::LoadRawData(EName::Type DeviceID, std::filesystem::path FilePath)
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[=]()
		{
			ThreadRequest_LoadRawData(DeviceID, FilePath);
		});
}
