#pragma once

#include <CoreMinimal.h>
#include "Event.h"
#include "Window.h"

class SViewerBase : public SWindow
{
	using Super = SWindow;
	using ThisClass = SViewerBase;
	using MenuBarHandler = std::function<void(void)>;

public:
	SViewerBase(EFont::Type _FontName, uint32_t _Width, uint32_t _Height);
	virtual ~SViewerBase() = default;

	void AddWindow(EName::Type WindowType, std::shared_ptr<SWindow> _Window, const FNativeDataInitialize& _Data, const std::any& Arg);
	void AppendWindows(const std::map<EName::Type, std::shared_ptr<SWindow>>& _Windows, const FNativeDataInitialize& _Data, const std::any& Arg);
	void SetMenuBar(MenuBarHandler Handler);

	virtual void Render() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Destroy() override;

	FEventSystem& GetEventSystem() { return EventSystem; }

private:
	bool IsExistsWindow(EName::Type WindowType, std::shared_ptr<SWindow> Window) const;

	FEventSystem EventSystem;
	MenuBarHandler Show_MenuBar;
	MenuBarHandler Input_HotKeys;
	std::unordered_map<EName::Type, std::vector<std::shared_ptr<SWindow>>> Windows;
};

class SViewerChildBase : public SWindow
{
	using Super = SWindow;
public:
	SViewerChildBase(FWindowInitializer& WindowInitializer)
		: SWindow(WindowInitializer)
	{}
	virtual ~SViewerChildBase()
	{
		UnsubscribeAll();
	}

	virtual EName::Type GetTag() const { return NAME_None; }

	std::shared_ptr<SViewerBase> GetParent() const
	{
		return std::dynamic_pointer_cast<SViewerBase>(Data.Parent.lock());
	}

	template <typename EventType>
	void SendEvent(const EventType& Event)
	{
		if (std::shared_ptr<SViewerBase> Parent = GetParent())
		{
			Parent->GetEventSystem().Publish(Event);
		}
	}

	template <typename EventType>
	void SubscribeEvent(std::function<void(const EventType&)> Handler, std::string_view Tag = "")
	{
		if (std::shared_ptr<SViewerBase> Parent = GetParent())
		{
			Parent->GetEventSystem().Subscribe<EventType>(this, std::move(Handler), Tag);
		}
	}

	void UnsubscribeAll()
	{
		if (std::shared_ptr<SViewerBase> Parent = GetParent())
		{
			Parent->GetEventSystem().Unsubscribe(this);
		}
	}
};
