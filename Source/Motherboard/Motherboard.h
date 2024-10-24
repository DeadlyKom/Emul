#pragma once

#include <CoreMinimal.h>
#include "Motherboard_Board.h"
#include "Devices/Device.h"

enum class FCPU_StepType;

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
	void Input_Step(FCPU_StepType Type);

	bool GetDebuggerState() const { return bFlipFlopDebugger; }
	void LoadRawData(EName::Type BoardID, EName::Type DeviceID, std::filesystem::path FilePath);
	
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

	template<typename T>
	void SetState(EName::Type BoardID, EName::Type DeviceID, const std::any& Value)
	{
		for (auto& [Name, Board] : Boards)
		{
			if (Board->UniqueBoardID != BoardID)
			{
				continue;
			}
			Board->SetState<T>(DeviceID, Value);
		}
	}

	// external signals
	void Reset();
	void NonmaskableInterrupt();

private:
	bool bFlipFlopDebugger;
	std::map<FName, std::shared_ptr<FBoard>> Boards;
};
