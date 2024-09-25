#pragma once

#include <CoreMinimal.h>

class FBoard;
class FDevice;

class FMotherboard
{
public:
	FMotherboard();
	
	void Initialize();
	void Shutdown();

	// motherboard setup
	FBoard& FindOrAddBoard(FName Name);
	void AddBoard(FName Name, std::vector<std::shared_ptr<FDevice>> _Devices, double Frequency);

	// external signals
	void Reset();
	void NonmaskableInterrupt();

private:
	std::map<FName, std::shared_ptr<FBoard>> Boards;
};
