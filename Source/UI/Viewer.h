#pragma once

#include <CoreMinimal.h>
#include "Window.h"

class FMotherboard;

enum class EWindowsType
{
	CPU_State,
	Disassembler,
};

class SViewer : public SWindow
{
public:
	SViewer(uint32_t _Width, uint32_t _Height);

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize() override;
	virtual void Render() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Destroy() override;

private:
	void Input_HotKeys();

	FMotherboard& GetMotherboard() const;

	// show functions
	void ShowMenu_File();
	void ShowMenu_Emulation();
	void ShowMenu_Windows();

	std::map<EWindowsType, std::shared_ptr<SWindow>> Windows;
};