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

void FBoard::Input_Step(FCPU_StepType Type)
{
	Thread->Input_Step(Type);
}

void FBoard::LoadRawData(EName::Type DeviceID, std::filesystem::path FilePath)
{
	Thread->LoadRawData(DeviceID, FilePath);
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

	std::string FormatFrequency;
	if (Frequency > 1e6)
	{
		FormatFrequency = std::format("{:.1f}M", Frequency * 1e-6);
	}
	else if (Frequency > 1e3)
	{
		FormatFrequency = std::format("{:.1f}K", Frequency * 1e-3);
	}
	else 
	{
		FormatFrequency = std::format("{}", Frequency);
	}


	LOG("[{}] : Set frequency: {}Hz", BoardName.ToString(), FormatFrequency.c_str());
}

void FBoard::Inut_Debugger(bool bEnterDebugger)
{
	Thread->Inut_Debugger(bEnterDebugger);
}
