#pragma once

#include <CoreMinimal.h>
#include "Motherboard_Thread.h"

class FDevice;
class FMotherboard;

enum class FCPU_StepType;

class FBoard
{
	friend FMotherboard;
public:
	FBoard(FName Name, EName::Type UniqueID);
	FBoard() = default;
	FBoard& operator=(const FBoard&) = delete;

	// board setup
	void AddDevices(std::vector<std::shared_ptr<FDevice>> _Devices, double _Frequency);
	void SetFrequency(double _Frequency);

	// input
	void Inut_Debugger(bool bEnterDebugger);

private:
	void Initialize();
	void Shutdown();

	// external signals
	void Reset();
	void NonmaskableInterrupt();

	// input
	void Input_Step(FCPU_StepType Type);

	void LoadRawData(EName::Type DeviceID, std::filesystem::path FilePath);
	template<typename T>
	T GetState(EName::Type DeviceID)
	{
		return Thread->GetState<T>(DeviceID);
	}

	template<typename T>
	void SetState(EName::Type DeviceID, const std::any& Value)
	{
		Thread->SetState<T>(DeviceID, Value);
	}

	FName BoardName;
	EName::Type UniqueBoardID;
	double Frequency;
	std::shared_ptr<FThread> Thread;
};
