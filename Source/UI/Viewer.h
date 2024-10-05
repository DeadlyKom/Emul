#pragma once

#include <CoreMinimal.h>
#include "Window.h"

class FMotherboard;
class SViewerChild;

enum class EWindowsType
{
	CPU_State,
	CallStack,
	MemoryDump,
	Disassembler,
};

class SViewer : public SWindow
{
	using Super = SWindow;
	using ThisClass = SViewer;
	friend SViewerChild;
public:
	SViewer(EFont::Type _FontName, uint32_t _Width, uint32_t _Height);

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize() override;
	virtual void Render() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Destroy() override;

private:
	void Input_HotKeys();

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
	{}

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
};
