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
	LOG(bFlipFlopDebugger ? "Enter debugger" : "Escepe debugger");

	for (auto& [Name, Board] : Boards)
	{
		if (Board) Board->Inut_Debugger(bFlipFlopDebugger);
	}
}

void FMotherboard::Input_Step(FCPU_StepType Type)
{
	if (!bFlipFlopDebugger)
	{
		return;
	}

	switch (Type)
	{
		case FCPU_StepType::StepTo:		LOG("Step to");		break;
		case FCPU_StepType::StepInto:	LOG("Step into");	break;
		case FCPU_StepType::StepOver:	LOG("Step over");	break;
		case FCPU_StepType::StepOut:	LOG("Step out");	break;
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
	LOG("Reset");

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
	
	LOG("Nonmaskable interrupt");

	for (auto& [Name, Board] : Boards)
	{
		if (Board) Board->NonmaskableInterrupt();
	}
}
