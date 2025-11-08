#pragma once

#include <CoreMinimal.h>
#include "Viewer.h"
#include <Core/Image.h>
#include "Devices/CPU/Interface_CPU_Z80.h"
#include "Devices/Memory/Interface_Memory.h"

class FMotherboard;
enum class EThreadStatus;
enum class FCPU_StepType;

enum class EDisassemblerInput
{
	None,
	MouseWheelUp,
	MouseWheelDown,
	PageUpPressed,
	PageDownPressed,

	Input_Enter,
	Input_UpArrow,
	Input_DownArrow,
	Input_CtrlUpArrow,
	Input_CtrlDownArrow,
	Input_CtrlLeftArrow,
	Input_CtrlRightArrow,
	Input_GoToAddress,
	GoToAddress,
};

enum class EDisassemblerInputValue
{
	ScrollingLines,
	GoTo_Address,
	GoTo_Line,
};

namespace DisassemblerColumn
{
	enum Type
	{
		Address = 0,
		Opcode,
		Instruction,

		None = INDEX_NONE
	};
}

struct FDisassemblerInput
{
	EDisassemblerInput Type;
	std::map<EDisassemblerInputValue, std::any> Value;
};

class SDisassembler : public SViewerChild, public IWindowEventNotification
{
	using Super = SViewerChild;
	using ThisClass = SDisassembler;
public:
	SDisassembler(EFont::Type _FontName);
	virtual ~SDisassembler() = default;

	virtual void Initialize(const std::any& Arg) override;
	virtual void SetupHotKeys() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Render() override;

private:
	FORCEINLINE FMotherboard& GetMotherboard() const;
	FORCEINLINE float InaccessibleHeight(int32_t LineNum) const;
	FORCEINLINE uint16_t GetProgramCounter() const;

	void Load_MemorySnapshot();
	void Upload_MemorySnapshot();

	void Draw_CodeDisassembler(EThreadStatus Status);
	void Draw_Breakpoint(uint16_t Address);
	void Draw_Address(uint16_t Address, int32_t CurrentLine);
	void Draw_OpcodeInstruction(uint16_t Address, const std::string& Opcodes, int32_t CurrentLine);
	void Draw_Instruction(uint16_t Address, const std::string& Command, int32_t CurrentLine);
	void Draw_ProgramCounter(uint16_t Address);

	void Enter_EditColumn();
	void Reset_EditColumn();
	void Prev_EditRow(bool bCtrl = false);
	void Next_EditRow(int32_t MaxLines, bool bCtrl = false);
	void Prev_EditColumn();
	void Next_EditColumn();

	void Input_HotKeys();
	void Input_Step(FCPU_StepType Type);
	void Input_Mouse();
	void Input_Enter();
	void Input_ShowNextStatement();
	void Input_UpArrow();
	void Input_DownArrow();	
	void Input_CtrlUpArrow();
	void Input_CtrlDownArrow();
	void Input_CtrlLeftArrow();
	void Input_CtrlRightArrow();
	void Input_PageUp();
	void Input_PageDown();
	void Input_GoToAddress();

	// events
	virtual void OnInputDebugger(bool bDebuggerState) override;

	// visual preferences
	bool bMemoryArea;
	bool bShowStatusBar;
	bool bShowOpcode;
	bool bAddressUpperCaseHex;			// display hexadecimal values as "FF" instead of "ff"
	bool bInstructionUpperCaseHex;		// display hexadecimal values as "FF" instead of "ff"

	// windows ID
	ImGuiID CodeDisassemblerID;
	float CodeDisassemblerScale;

	// image 
	FImageHandle ImageProgramCounter;

	// state
	bool bEditingTakeFocusReset;
	bool bAddressEditingTakeFocus;
	bool bOpcodeInstructionEditingTakeFocus;
	bool bInstructionEditingTakeFocus;
	char AddressInputBuffer[9];
	char OpcodeInstructioBuffer[256];
	int32_t AddressEditing;
	int32_t OpcodeInstructionEditing;
	int32_t InstructionEditing;

	FDisassemblerInput InputActionEvent;

	int32_t TopCursorAtAddress;
	int32_t UserCursorAtAddress;
	int32_t UserCursorAtLine;
	int32_t UserCursorAtColumn;

	uint64_t TimeElapsedCounter;
	uint64_t LatestClockCounter;
	EThreadStatus Status;
	FMemorySnapshot Snapshot;
	std::vector<uint8_t> AddressSpace;
};
