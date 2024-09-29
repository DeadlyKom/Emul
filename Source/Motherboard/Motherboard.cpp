#include "Motherboard.h"
#include "AppFramework.h"
#include "Devices/CPU/Z80.h"

FMotherboard::FMotherboard()
	: bFlipFlopDebugger(false)
{}

void FMotherboard::Initialize()
{}

void FMotherboard::Shutdown()
{
	for (auto& [Name, Board] : Boards)
	{
		if (Board) Board->Shutdown();
	}
}

FBoard& FMotherboard::FindOrAddBoard(FName Name, EName::Type UniqueID)
{
	if (Boards.contains(Name))
	{
		return *Boards[Name];
	}

	Boards.emplace(Name, std::make_shared<FBoard>(Name, UniqueID));
	Boards[Name]->Initialize();
	return *Boards[Name];
}

void FMotherboard::AddBoard(FName Name, EName::Type UniqueID, std::vector<std::shared_ptr<FDevice>> _Devices, double Frequency)
{
	FindOrAddBoard(Name, UniqueID).AddDevices(_Devices, Frequency);
}

void FMotherboard::Inut_Debugger()
{
	bFlipFlopDebugger = !bFlipFlopDebugger;
	LOG_CONSOLE(bFlipFlopDebugger ? "Enter debugger" : "Escepe debugger");

	for (auto& [Name, Board] : Boards)
	{
		if (Board) Board->Inut_Debugger(bFlipFlopDebugger);
	}
}

void FMotherboard::Input_Step(FCPU_StepType::Type Type)
{
	if (!bFlipFlopDebugger)
	{
		return;
	}

	switch (Type)
	{
	case FCPU_StepType::StepTo:		LOG_CONSOLE("Step to");		break;
	case FCPU_StepType::StepInto:	LOG_CONSOLE("Step into");	break;
	case FCPU_StepType::StepOver:	LOG_CONSOLE("Step over");	break;
	case FCPU_StepType::StepOut:	LOG_CONSOLE("Step out");	break;
	}

	for (auto& [Name, Board] : Boards)
	{
		if (Board) Board->Input_Step(Type);
	}
}

void FMotherboard::LoadRawData(EName::Type BoardID, EName::Type DeviceID, std::filesystem::path FilePath)
{
	for (auto& [Name, Board] : Boards)
	{
		if (Board->UniqueBoardID != BoardID)
		{
			continue;
		}
		Board->LoadRawData(DeviceID, FilePath);
	}
}

void FMotherboard::Reset()
{
	LOG_CONSOLE("Reset");

	for (auto& [Name, Board] : Boards)
	{
		if (Board) Board->Reset();
	}
}

void FMotherboard::NonmaskableInterrupt()
{
	if (bFlipFlopDebugger)
	{
		return;
	}
	
	LOG_CONSOLE("Nonmaskable interrupt");

	for (auto& [Name, Board] : Boards)
	{
		if (Board) Board->NonmaskableInterrupt();
	}
}
