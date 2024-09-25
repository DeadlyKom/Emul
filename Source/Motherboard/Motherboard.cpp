#include "Motherboard.h"
#include "AppFramework.h"
#include "Devices/Device.h"
#include "Motherboard_Board.h"

FMotherboard::FMotherboard()
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

FBoard& FMotherboard::FindOrAddBoard(FName Name)
{
	if (Boards.contains(Name))
	{
		return *Boards[Name];
	}

	Boards.emplace(Name, std::make_shared<FBoard>(Name));
	Boards[Name]->Initialize();
	return *Boards[Name];
}

void FMotherboard::AddBoard(FName Name, std::vector<std::shared_ptr<FDevice>> _Devices, double Frequency)
{
	FindOrAddBoard(Name).AddDevices(_Devices, Frequency);
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
	LOG_CONSOLE("NonmaskableInterrupt");

	for (auto& [Name, Board] : Boards)
	{
		if (Board) Board->NonmaskableInterrupt();
	}
}
