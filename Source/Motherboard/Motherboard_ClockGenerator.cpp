#include "Motherboard_ClockGenerator.h"

FClockGenerator::FClockGenerator()
	: Sampling(1)
	, FrequencyInv(1.0 / 3.5_MHz)
{}

void FClockGenerator::Tick()
{
	ClockCounter++;

	Events.erase(std::remove_if(Events.begin(), Events.end(), 
		[=](const FEventData& Event)
		{
			if (Event.ExpireTime < ClockCounter)
			{
				Event.Callback();
				return true;
			}
			return false;
		}), Events.end());
}

void FClockGenerator::Reset()
{
	ClockCounter = -1;
	Events.clear();
}

void FClockGenerator::AddEvent(uint64_t Rate, std::function<void()>&& EventCallback, const std::string& _DebugName /*= ""*/)
{
	FEventData Event
	{
		.ExpireTime = ClockCounter + Rate,
		.DebugName = _DebugName,
		.Callback = std::move(EventCallback),
	};

	Events.push_back(Event);
}
