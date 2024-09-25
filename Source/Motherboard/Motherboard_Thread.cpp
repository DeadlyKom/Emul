#include "Motherboard_Thread.h"
#include "Devices/Device.h"
#include "AppFramework.h"

FThread::FThread(FName Name)
	: ThreadName(Name)
	, ThreadStatus(EThreadStatus::Stop)
{}

void FThread::Initialize()
{
	Thread = std::thread(&FThread::Thread_Execution, this);
}

void FThread::Shutdown()
{
	Thread_Request(EThreadTypeRequest::ExecuteTask,
		[this]() -> void
		{
			Device_Unregistration();
		});
	Thread_Request(EThreadTypeRequest::Quit);
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
			CG.SetFrequency(Frequency * 2.0 /*half-cycle*/);
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
			Device->Register(SB);
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
		if (Device) Device->Unregister(SB);
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

void FThread::Thread_Request(EThreadTypeRequest TypeRequest, std::function<void()>&& Task/* = nullptr*/)
{
	ThreadRequest.Push({ TypeRequest, std::move(Task) });
}

void FThread::Thread_Execution()
{
	LOG_CONSOLE("[{}] : Thread started.", ThreadName.ToString());

	// main loop
	while (ThreadStatus != EThreadStatus::Quit)
	{
		while (ThreadStatus == EThreadStatus::Run)
		{
			TM.Tick();		// internal auxiliary time manager
			CG.Tick();		// internal clock generator

			for (std::shared_ptr<FDevice>& Device : Devices)
			{
				if (Device) Device->Tick(CG, SB);
			}

			Thread_RequestHandling();
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
		case EThreadTypeRequest::Run:					ThreadRequest_SetStatus(EThreadStatus::Run);	break;
		case EThreadTypeRequest::Stop:					ThreadRequest_SetStatus(EThreadStatus::Stop);	break;
		case EThreadTypeRequest::Quit:					ThreadRequest_SetStatus(EThreadStatus::Quit);	break;
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

void FThread::ThreadRequest_ExecuteTask(const  std::function<void()>&& Task)
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
	TM.SetTimer(0.2f,
		[&]()
		{
			SB.SetInactive(BUS_RESET);
		}, false, "End signal RESET");
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
