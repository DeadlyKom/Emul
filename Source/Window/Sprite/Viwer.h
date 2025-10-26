#pragma once

#include <CoreMinimal.h>
#include "Window.h"
#include "Interface_WindowEventNotification.h"

class FAppDebugger;
class FMotherboard;
class SViewerChild;

enum class EWindowsType
{
	Screen,
	CPU_State,
	CallStack,
	MemoryDump,
	Disassembler,
	Oscillograph,
};

class SViewer : public SWindow
{
	using Super = SWindow;
	using ThisClass = SViewer;
	friend SViewerChild;
	friend FAppDebugger;

public:
	SViewer(EFont::Type _FontName, uint32_t _Width, uint32_t _Height);
	virtual ~SViewer() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize() override;
	virtual void Render() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Destroy() override;

private:
	void Input_HotKeys();
	void Inut_Debugger();

	template <typename... Args>
	void SendEventNotification(EEventNotificationType EventType, Args&&... args)
	{
		using ArgType = std::decay_t<decltype(args)...>;

		for (auto& [Type, Window] : Windows)
		{
			std::shared_ptr<IWindowEventNotification> Interface = std::dynamic_pointer_cast<IWindowEventNotification>(Window);
			if (Interface != nullptr)
			{
				switch (EventType)
				{
				case EEventNotificationType::Input_Step:
					if constexpr (sizeof...(args) == 1)
					{
						if constexpr (std::is_convertible_v<ArgType, FCPU_StepType>)
						{
							Interface->OnInputStep(std::forward<Args>(args)...);
						}
					}
					break;
				case EEventNotificationType::Input_Debugger:
					if constexpr (sizeof...(args) == 1)
					{
						if constexpr (std::is_convertible_v<ArgType, bool>)
						{
							Interface->OnInputDebugger(std::forward<Args>(args)...);
						}
					}
					break;
				case EEventNotificationType::ForceUpdateStates:
					if constexpr (sizeof...(args) == 0)
					{
						Interface->OnForceUpdateStates();
					}
					break;
				}
			}
		}
	}

	FMotherboard& GetMotherboard() const;
	std::shared_ptr<SWindow> GetWindow(EWindowsType Type)
	{
		const std::map<EWindowsType, std::shared_ptr<SWindow>>::iterator& SearchIt = Windows.find(Type);
		return SearchIt != Windows.end() ? SearchIt->second : nullptr;
	}

	// show functions
	void ShowMenu_File();
	void ShowMenu_Emulation();
	void ShowMenu_Windows();

	std::map<EWindowsType, std::shared_ptr<SWindow>> Windows;
};

class SViewerChild : public SWindow
{
	using Super = SWindow;
public:
	SViewerChild(FWindowInitializer& WindowInitializer)
		: SWindow(WindowInitializer)
	{
	}
	virtual ~SViewerChild() = default;

	std::shared_ptr<SViewer> GetParent() const
	{
		return std::dynamic_pointer_cast<SViewer>(Data.Parent);
	}

	template <typename T>
	std::shared_ptr<T> GetWindow(EWindowsType Type)
	{
		std::shared_ptr<SViewer> Viewer = GetParent();
		assert(Viewer);
		return Viewer ? std::dynamic_pointer_cast<T>(Viewer->GetWindow(Type)) : nullptr;
	}

	template <typename... Args>
	void SendEventNotification(EEventNotificationType EventType, Args&&... args)
	{
		std::shared_ptr<SViewer> Viewer = GetParent();
		if (Viewer)
		{
			Viewer->SendEventNotification(EventType, std::forward<Args>(args)...);
		}
	}
};
