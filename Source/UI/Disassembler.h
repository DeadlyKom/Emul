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
	PageUpPressed,
	PageDownPressed,

	Input_UpArrow,
	Input_DownArrow,
	Input_GoToAddress,
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
	void Draw_OpcodeInstruction(uint16_t Address, const std::string& Opcodes, int32_t CurrentLine);
	void Draw_Instruction(uint16_t Address, const std::string& Command, int32_t CurrentLine);

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
	bool bShowOpcode;
	bool bAddressUpperCaseHex;			// display hexadecimal values as "FF" instead of "ff"
	bool bInstructionUpperCaseHex;		// display hexadecimal values as "FF" instead of "ff"

	// windows ID
	ImGuiID CodeDisassemblerID;
	float CodeDisassemblerScale;

	// state
	bool bAddressEditingTakeFocus;
	bool bOpcodeInstructionEditingTakeFocus;
	bool bInstructionEditingTakeFocus;
	char AddressInputBuffer[9];
	char OpcodeInstructioBuffer[256];
	int32_t AddressEditing;
	int32_t OpcodeInstructionEditing;
	int32_t InstructionEditing;

	FDisassemblerInput InputActionEvent;

	uint16_t TopCursorAtAddress;
	uint16_t UserCursorAtAddress;
	int32_t UserCursorAtLine;

	uint64_t LatestClockCounter;
	FMemorySnapshot Snapshot;
	std::vector<uint8_t> AddressSpace;
};
