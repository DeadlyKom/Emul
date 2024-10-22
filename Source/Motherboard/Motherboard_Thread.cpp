#include "Motherboard_Thread.h"
#include "Devices/Device.h"
#include "AppFramework.h"

#include "Devices/CPU/Z80.h"
#include "Devices/CPU/Interface_CPU_Z80.h"
#include "Devices/Memory/Interface_Memory.h"
#include "Devices/ControlUnit/Interface_Display.h"

#include "Utils/ProfilerScope.h"

FThread::FThread(FName Name)
	: ThreadName(Name)
	, bInterruptLatch(false)
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
			for (std::shared_ptr<FDevice>& Device : Devices)
			{
				Device->CalculateFrequency(Frequency, CG.GetSampling());
			}
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
	for (const std::shared_ptr<FDevice>& Device : _Devices)
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
			LOG("[{}] : The device cannot be reinitialized.", ThreadName.ToString());
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
	return Result;
}

void FThread::Thread_Request(EThreadTypeRequest TypeRequest, Callback&& Task/* = nullptr*/)
{
	ThreadRequest.Push({ TypeRequest, std::move(Task) });
}

void FThread::Device_ThreadRequest(EName::Type DeviceID, const std::type_index& Type, std::any Value)
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[=, this]()
		{
			SetState_RequestHandler(DeviceID, Type, Value);
		});
}

std::any FThread::Device_ThreadRequestResult(EName::Type DeviceID, const std::type_index& Type)
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[=, this]()
		{
			GetState_RequestHandler(DeviceID, Type);
		});

	return ThreadRequestResult.Pop();
}

void FThread::Thread_Execution()
{
	LOG("[{}] : Thread started.", ThreadName.ToString());

	// main loop
	while (ThreadStatus != EThreadStatus::Quit)
	{
		while (ThreadStatus <= EThreadStatus::Stop)
		{
			TM.Tick();
			Thread_RequestHandling();
		}

		if (!SerializedDataCPU.empty())
		{
			DeserializeCPU(SerializedDataCPU);
			SerializedDataCPU.clear();
		}

		std::chrono::system_clock::time_point Frame_StartTime = std::chrono::system_clock::now();

		PROFILER_SCOPE(INDEX_NONE, [&, this]() -> bool
			{
				if (ThreadStatus == EThreadStatus::Run)
				{
					
					return true;
				}
				for (std::shared_ptr<FDevice>& Device : Devices)
				{
					if (Device->DeviceType != EDeviceType::CPU)
					{
						continue;
					}
					if (std::shared_ptr<ICPU_Z80> CPU = std::dynamic_pointer_cast<ICPU_Z80>(Device))
					{
						return !CPU->IsInstrCycleDone();
					}
				}
				return false;
			})
		{

			CG.Tick();		// internal clock generator
			for (std::shared_ptr<FDevice>& Device : Devices)
			{
				if (Device) Device->MainTick();
			}

			// check request at end of frame
			const bool bIsInterrupt = SB.IsPositiveEdge(BUS_INT);
			if (bInterruptLatch != bIsInterrupt)
			{
				bInterruptLatch = bIsInterrupt;
				if (bInterruptLatch)
				{
					const std::chrono::system_clock::time_point Frame_EndTime = std::chrono::system_clock::now();
					const std::chrono::duration<double, std::milli> ElapsedTime = Frame_EndTime - Frame_StartTime;
					LOG("Frame Time: {:0.1f} ms", ElapsedTime.count());
					Frame_StartTime = Frame_EndTime;

					const double DesiredFrameTime = 1000.0 / 50.0;
					double SleepTime = DesiredFrameTime - ElapsedTime.count();

					std::chrono::system_clock::time_point SyncMainFrame_StartTime = Frame_EndTime;
					do
					{
						Thread_RequestHandling();
						if (SleepTime > 0)
						{
							std::this_thread::sleep_until(SyncMainFrame_StartTime + std::chrono::microseconds(100));
						}

						const std::chrono::system_clock::time_point SyncMainFrame_EndTime = std::chrono::system_clock::now();
						const std::chrono::duration<double, std::milli> ElapsedTime = SyncMainFrame_EndTime - SyncMainFrame_StartTime;
						SleepTime -= ElapsedTime.count();
					} while (SleepTime > 0);
				}
			}
		};

		while (ThreadStatus == EThreadStatus::Trace)
		{
			CG.Tick();		// internal clock generator

			bool bStopTrace = false;
			for (std::shared_ptr<FDevice> Device : Devices)
			{
				if (Device)
				{
					bStopTrace |= Device->TickStopCondition(
						[this](std::shared_ptr<FDevice> _Device) -> bool
						{
							return ThreadRequest_StopCondition(_Device);
						});
				}
			}
			if (bStopTrace)
			{
				StepType = FCPU_StepType::None; ThreadRequest_SetStatus(EThreadStatus::Stop);
			}
		}
	};

	LOG("[{}] : Thread shutdown.", ThreadName.ToString());
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
	std::shared_ptr<ICPU_Z80> CPU = std::dynamic_pointer_cast<ICPU_Z80>(Device);
	if (CPU == nullptr)
	{
		return false;
	}

	bool bInstrCycleDone = CPU->IsInstrCycleDone();
	if (bInstrCycleDone)
	{
		SerializeCPU(SerializedDataCPU);
		bInstrCycleDone = CPU->Flush();
	}
	return bInstrCycleDone;

	//switch (StepType)
	//{
	//case FCPU_StepType::StepTo:
	//	break;
	//case FCPU_StepType::StepInto:
	//	break;
	//case FCPU_StepType::StepOver:
	//	break;
	//case FCPU_StepType::StepOut:
	//	break;
	//}
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
	ADD_EVENT(CG, 8 * CG.GetSampling(), 0,
		[&]()
		{
			SB.SetInactive(BUS_RESET);
		}, "End signal RESET");
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
		case NAME_None:
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
		case NAME_Z80:
		{
			if (Type == typeid(double))
			{
				double Frequency = 0.0f;
				ICPU_Z80* ICPU = GetDevice<ICPU_Z80>();
				if (ICPU == nullptr)
				{
					LOG_ERROR("[{}]\t failed to find device.", (__FUNCTION__));
				}
				else
				{
					Frequency = ICPU->GetFrequency();
				}
				return ThreadRequestResult.Push(Frequency);
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
						if (std::shared_ptr<ICPU_Z80> CPU = std::dynamic_pointer_cast<ICPU_Z80>(Device))
						{
							return ThreadRequestResult.Push(CPU->GetRegisters());
						}
					}
				}
				break;
			}
			break;
		}
		case NAME_Memory:
		{
			FMemorySnapshot MS(CG.GetClockCounter());
			IMemory* Memory = GetDevice<IMemory>();
			if (Memory == nullptr)
			{
				LOG_ERROR("[{}]\t failed to find device.", (__FUNCTION__));
			}
			else
			{
				Memory->Snapshot(MS, EMemoryOperationType::Read);
			}
			return ThreadRequestResult.Push(MS);
		}
		case NAME_ULA:
		{
			if (Type == typeid(FSpectrumDisplay))
			{
				FSpectrumDisplay SD;
				IDisplay* Display = GetDevice<IDisplay>();
				if (Display == nullptr)
				{
					LOG_ERROR("[{}]\t failed to find device.", (__FUNCTION__));
				}
				else
				{
					Display->GetSpectrumDisplay(SD);
				}
				return ThreadRequestResult.Push(SD);
			}
		}
		case NAME_Display: // ?????????
		{
			if (Type == typeid(FSpectrumDisplay))
			{
				FSpectrumDisplay SD = GetFromContainer<FSpectrumDisplay>(Type);
				return ThreadRequestResult.Push(SD);
			}
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
		[=, this]()
		{
			ThreadRequest_LoadRawData(DeviceID, FilePath);
		});
}

void FThread::SerializeCPU(std::string& Output)
{
	ICPU_Z80* ICPU = GetDevice<ICPU_Z80>();
	if (ICPU == nullptr)
	{
		LOG_ERROR("[{}]\t failed to find device.", (__FUNCTION__));
		return;
	}
	std::ostringstream os(std::ios::binary);
	ICPU->Serialize(os);
	Output = os.str();
}

void FThread::DeserializeCPU(const std::string& Input)
{
	ICPU_Z80* ICPU = GetDevice<ICPU_Z80>();
	if (ICPU == nullptr)
	{
		LOG_ERROR("[{}]\t failed to find device.", (__FUNCTION__));
		return;
	}

	std::istringstream is(SerializedDataCPU);
	ICPU->Deserialize(is);
}
