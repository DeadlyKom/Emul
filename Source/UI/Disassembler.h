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

	InputGoToAddress,
	GoToAddress,
};

enum class EDisassemblerInputValue
{
	ScrollingLines,
	GoTo_Address,
	GoTo_Line,
};

struct FDisassemblerInput
{
	EDisassemblerInput Type;
	std::map<EDisassemblerInputValue, std::any> Value;
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
	void Draw_Address(uint16_t Address, int32_t CurrentLine);
	uint32_t Draw_Instruction(uint16_t& Address);

	void Input_HotKeys();
	void Input_Mouse();
	void Input_UpArrow();
	void Input_DownArrow();
	void Input_PageUp();
	void Input_PageDown();
	void Input_GoToAddress();

	// visual preferences
	bool bMemoryArea;
	bool bShowStatusBar;
	bool bAddressUpperCaseHex;			// display hexadecimal values as "FF" instead of "ff"
	bool bInstructionUpperCaseHex;		// display hexadecimal values as "FF" instead of "ff"

	// windows ID
	ImGuiID CodeDisassemblerID;
	float CodeDisassemblerScale;

	// state
	bool bAddressEditingTakeFocus;
	char AddressInputBuffer[9];
	int32_t AddressEditing;

	FDisassemblerInput InputActionEvent;

	uint16_t CursorAtAddress;

	uint64_t LatestClockCounter;
	FMemorySnapshot Snapshot;
	std::vector<uint8_t> AddressSpace;
};
