#include "Motherboard_ClockGenerator.h"

namespace
{
	constexpr size_t CapacityStep = 128;
}

FClockGenerator::FClockGenerator()
	: Sampling(1)
	, FrequencyInv(1.0 / 3.5_MHz)
	, ClockCounter(-1)
	, ElementCount(CapacityStep)
	, LastElementIndex(0)
{
	Events.resize(CapacityStep);
}

FClockGenerator::~FClockGenerator()
{
	// ToDo remove all events
}

void FClockGenerator::Tick()
{
	ClockCounter++;

	for (size_t i = 0; i < LastElementIndex; ++i)
	{
		const FEventData& Event = Events[i];
		if (Event.ExpireTime > ClockCounter)
		{
			continue;
		}
		if (Event.Callback) Event.Callback();
		if (i + 1 < LastElementIndex)
		{
			std::swap(Events[i], Events[LastElementIndex-1]);
			i--;
		}
		LastElementIndex--;
	}
}

void FClockGenerator::Reset()
{
	ClockCounter = -1;
	LastElementIndex = 0;
}

#ifndef NDEBUG
void FClockGenerator::AddEvent(uint64_t Rate, std::function<void()>&& EventCallback, const std::string& _DebugName /*= ""*/)
#else
void FClockGenerator::AddEvent(uint64_t Rate, std::function<void()>&& EventCallback)
#endif 
{
	if (LastElementIndex > ElementCount)
	{
		ElementCount += CapacityStep;
		Events.resize(ElementCount);
	}

	FEventData* Event = &Events[LastElementIndex++];
	Event->ExpireTime = ClockCounter + Rate + (ClockCounter == -1 ? 1 : 0);
#ifndef NDEBUG
	Event->DebugName = _DebugName;
#endif
	Event->Callback = std::move(EventCallback);
}
