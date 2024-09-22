#pragma once

#include <CoreMinimal.h>

class FDevice;
class FThread;
class FMotherboard;

class FBoard
{
	friend FMotherboard;
public:
	FBoard(FName Name);
	FBoard() = default;
	FBoard& operator=(const FBoard&) = delete;

	// board setup
	void AddDevices(const std::vector<std::shared_ptr<FDevice>>& _Devices, double _Frequency);
	void SetFrequency(double _Frequency);

private:
	void Initialize();
	void Shutdown();

	// external signals
	void Reset();
	void NonmaskableInterrupt();

	FName BoardName;
	double Frequency;
	std::shared_ptr<FThread> Thread;
};
