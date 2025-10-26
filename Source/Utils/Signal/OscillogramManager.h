#pragma once

#include <CoreMinimal.h>
#include "SystemTime.h"

namespace ESignalState { enum Type; }
namespace ESignalBus { enum Type; }

struct FOscillogramSignal
{
	FOscillogramSignal();

	int64_t Time;
	ESignalState::Type State;
};

class FOscillogramManager
{
public:
	FOscillogramManager();

	void Initialize(int32_t SignalNum, int32_t NewSampleRateCapacity);
	void SetSignal(ESignalBus::Type Signal, ESignalState::Type State);
	const std::vector<FOscillogramSignal>* GetSignals(ESignalBus::Type SignalName) const;

private:
	FSystemTime Time;
	int32_t SampleRateCapacity;
	std::vector<uint32_t> OscillogramCounter;
	std::map<ESignalBus::Type, std::vector<FOscillogramSignal>> Oscillogram;
};