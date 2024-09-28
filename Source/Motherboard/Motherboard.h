#pragma once

#include <CoreMinimal.h>
#include "Motherboard_Board.h"
#include "Devices/Device.h"

namespace FCPU_StepType { enum Type; }

class FMotherboard
{
public:
	FMotherboard();
	
	void Initialize();
	void Shutdown();

	// motherboard setup
	FBoard& FindOrAddBoard(FName Name, EName::Type UniqueID);
	void AddBoard(FName Name, EName::Type UniqueID, std::vector<std::shared_ptr<FDevice>> _Devices, double Frequency);

	// input
	void Inut_Debugger();
	void Input_Step(FCPU_StepType::Type Type);

	template<typename T>
	T GetState(EName::Type BoardID, EName::Type DeviceID)
	{
		for (auto& [Name, Board] : Boards)
		{
			if (Board->UniqueBoardID != BoardID)
			{
				continue;
			}
			return Board->GetState<T>(DeviceID);
		}
		return T();
	}

	// external signals
	void Reset();
	void NonmaskableInterrupt();

private:
	bool bFlipFlopDebugger;
	std::map<FName, std::shared_ptr<FBoard>> Boards;
};
