#pragma once

#include <CoreMinimal.h>
#include "Viewer.h"
#include "Devices/Memory/Interface_Memory.h"

class FMotherboard;
enum class EThreadStatus;

class SMemoryDump : public SViewerChild
{
	using Super = SViewerChild;
	using ThisClass = SMemoryDump;
public:
	SMemoryDump(EFont::Type _FontName);

	virtual void Tick(float DeltaTime) override;
	virtual void Render() override;

private:
	FORCEINLINE FMotherboard& GetMotherboard() const;
	FORCEINLINE float InaccessibleHeight(int32_t LineNum, int32_t SeparatorNum) const;

	void Load_MemorySnapshot();

	void Draw_DumpMemory();

	void Input_HotKeys();
	void Input_Mouse();

	// visual preferences
	bool bMemoryArea;
	bool bASCII_Values;
	bool bShowStatusBar;
	int32_t ColumnsToDisplay;

	// windows ID
	ImGuiID MemoryDumpID;
	float MemoryDumpScale;

	bool DataEditingTakeFocus;
	int32_t BaseDisplayAddress;
	int32_t DataEditingAddress;
	int32_t DataPreviewAddress;

	uint64_t LatestClockCounter;
	EThreadStatus Status;
	FMemorySnapshot Snapshot;
	std::vector<uint8_t> AddressSpace;
};
