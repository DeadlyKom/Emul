#pragma once

#include <CoreMinimal.h>
#include "Window.h"
#include "Devices/Memory/Interface_Memory.h"

class FMotherboard;
enum class EThreadStatus;

enum class EDisassemblerInput
{
	None,
	MouseWheelUp,
	MouseWheelDown,
	PageUpArrow,
	PageDownArrow,
	PageUpPressed,
	PageDownPressed,
};

class SDisassembler : public SWindow
{
	using ThisClass = SDisassembler;
public:
	SDisassembler(EFont::Type _FontName);

	virtual void Initialize() override;
	virtual void Render() override;

private:
	FORCEINLINE FMotherboard& GetMotherboard() const;

	void Update_MemorySnapshot();

	void Draw_CodeDisassembler(EThreadStatus Status);
	void Draw_Breakpoint(uint16_t Address);
	void Draw_Address(uint16_t Address);
	uint32_t Draw_Instruction(uint16_t& Address);

	void Input_HotKeys();
	void Input_Mouse();
	void Input_UpArrow();
	void Input_DownArrow();
	void Input_PageUp();
	void Input_PageDown();

	// visual preferences
	bool bMemoryArea;
	bool bShowStatusBar;

	// windows ID
	ImGuiID CodeDisassemblerID;
	float CodeDisassemblerScale;

	// state
	float MouseWheel;
	EDisassemblerInput ActionInput;

	uint16_t PrevCursorAtAddress;
	uint16_t CursorAtAddress;

	uint64_t LatestClockCounter;
	FMemorySnapshot Snapshot;
	std::vector<uint8_t> AddressSpace;
};
