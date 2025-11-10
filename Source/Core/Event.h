#pragma once

#include <CoreMinimal.h>

namespace FEventTag
{
	static const FName AnyTag = TEXT("Any");
}

struct IEvent
{
	FName Tag = FEventTag::AnyTag;
	virtual ~IEvent() = default;
};

using EventHandler = std::function<void(const IEvent&)>;

class FEventSystem
{
public:
	template <typename EventType>
	void Subscribe(void* Owner, std::function<void(const EventType&)> Handler, std::string_view Tag = "")
	{
		auto Wrapper = [Handler = std::move(Handler)](const IEvent& Event)
			{
				if (auto* Casted = dynamic_cast<const EventType*>(&Event))
				{
					Handler(*Casted);
				}
			};

		Subscribers.emplace_back(FSubscriber{ Owner, std::string(Tag), std::move(Wrapper) });
	}

	void Unsubscribe(void* Owner)
	{
		std::erase_if(Subscribers, [Owner](const FSubscriber& S) { return S.Owner == Owner; });
	}

	void Publish(const IEvent& Event)
	{
		const std::vector<FSubscriber> _Subscribers = Subscribers;
		for (const FSubscriber& S : _Subscribers)
		{
			if (Event.Tag || S.Tag == Event.Tag)
			{
				S.Handler(Event);
			}
		}
	}

private:
	struct FSubscriber
	{
		void* Owner;
		std::string Tag;
		EventHandler Handler;
	};

	std::vector<FSubscriber> Subscribers;
};
