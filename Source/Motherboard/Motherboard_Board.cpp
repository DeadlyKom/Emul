#include "Motherboard_Board.h"
#include "AppFramework.h"
#include "Devices/CPU/Z80.h"

FBoard::FBoard(FName Name, EName::Type UniqueID)
	: BoardName(Name)
	, UniqueBoardID(UniqueID)
{}

void FBoard::Initialize()
{
	Thread = std::make_shared<FThread>(BoardName);
	Thread->Initialize();
}

void FBoard::Shutdown()
{
	Thread->Shutdown();
}

void FBoard::Reset()
{
	Thread->Reset();
}

void FBoard::NonmaskableInterrupt()
{
	Thread->NonmaskableInterrupt();
}

void FBoard::Input_Step(FCPU_StepType::Type Type)
{
	Thread->Input_Step(Type);
}

void FBoard::AddDevices(std::vector<std::shared_ptr<FDevice>> Devices, double _Frequency)
{
	Thread->AddDevices(Devices);
	SetFrequency(_Frequency);
}

void FBoard::SetFrequency(double _Frequency)
{
	if (Frequency == _Frequency)
	{
		return;
	}

	Frequency = _Frequency;
	Thread->SetFrequency(Frequency);

	LOG_CONSOLE("[{}] : Set frequency: {}Hz", BoardName.ToString(), Frequency);
}

void FBoard::Inut_Debugger(bool bEnterDebugger)
{
	Thread->Inut_Debugger(bEnterDebugger);
}
