#pragma once

#include <CoreMinimal.h>
#include "Window.h"
#include "Devices/Memory/Interface_Memory.h"

class FMotherboard;
enum class EThreadStatus;

class SDisassembler : public SWindow
{
public:
	SDisassembler(EFont::Type _FontName);

	virtual void Initialize() override;
	virtual void Render() override;

private:
	FORCEINLINE FMotherboard& GetMotherboard() const;

	void Update_MemorySnapshot();

	void Draw_CodeDisassembler(EThreadStatus Status);
	void Draw_Breakpoint(uint32_t Address);
	void Draw_Address(uint32_t Address);
	uint32_t Draw_Mnemonic(uint32_t& Address);

	void Input_HotKeys();

	// visual preferences
	bool bMemoryArea;

	uint32_t CursorAtAddress;

	uint64_t LatestClockCounter;
	FMemorySnapshot Snapshot;
	std::vector<uint8_t> AddressSpace;
};
