#include "OscillogramManager.h"
#include "Utils/Signal/Bus.h"

FOscillogramSignal::FOscillogramSignal()
	: Time(-1)
	, State(ESignalState::HiZ)
{}

FOscillogramManager::FOscillogramManager()
	: SampleRateCapacity(128)
{
	Time.Initialize();
}

void FOscillogramManager::Initialize(int32_t SignalNum, int32_t NewSampleRateCapacity)
{
	SampleRateCapacity = NewSampleRateCapacity;

	OscillogramCounter.resize(SignalNum);
	memset(OscillogramCounter.data(), 0, SignalNum * sizeof(int32_t));
}

void FOscillogramManager::SetSignal(ESignalBus::Type SignalName, ESignalState::Type State)
{
	if (!Oscillogram.contains(SignalName))
	{
		Oscillogram.emplace(SignalName, std::vector<FOscillogramSignal>(SampleRateCapacity));
	}

	uint32_t& CurrentIndex = OscillogramCounter[SignalName];
	const uint32_t PrevIndex = CurrentIndex - 1;
	const FOscillogramSignal& PrevSignal = Oscillogram[SignalName][PrevIndex % SampleRateCapacity];
	if (PrevSignal.State == State)
	{
		return;
	}

	FOscillogramSignal& CurrentSignal = Oscillogram[SignalName][CurrentIndex++ % SampleRateCapacity];
	CurrentSignal.Time = Time.GetCurrentTick();
	CurrentSignal.State = State;
}

const std::vector<FOscillogramSignal>* FOscillogramManager::GetSignals(ESignalBus::Type SignalName) const
{
	return Oscillogram.contains(SignalName) ? &Oscillogram.find(SignalName)->second : nullptr;
}

