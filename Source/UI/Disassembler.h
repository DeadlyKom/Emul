#pragma once

#include <CoreMinimal.h>
#include "Window.h"
#include "Devices/Memory/Interface_Memory.h"

class FMotherboard;
enum class EThreadStatus;

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

	// visual preferences
	bool bMemoryArea;
	bool bShowStatusBar;

	// windows ID
	ImGuiID CodeDisassemblerID;
	float CodeDisassemblerScale;

	// state
	float MouseWheel;

	uint16_t PrevCursorAtAddress;
	uint16_t CursorAtAddress;

	uint64_t LatestClockCounter;
	FMemorySnapshot Snapshot;
	std::vector<uint8_t> AddressSpace;
};
