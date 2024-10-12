#pragma once

#include <CoreMinimal.h>

enum class EDataBlockState
{
	Unknown,
	Actived,
};

enum class EMemoryOperationType
{
	Read,
	Write
};

struct FDataBlock
{
	FName DeviceName;
	FName BlockName;
	bool bReadOnlyMode;
	EDataBlockState State;
	uint32_t PlacementAddress;
	std::vector<uint8_t> Data;
};

struct FMemorySnapshot
{
	FMemorySnapshot(uint64_t SnapshotTime = INDEX_NONE)
		: SnapshotTimeCC (SnapshotTime)
	{}
	void AddDataBlock(const FDataBlock& DataBlock)
	{
		DataBlocks.push_back(DataBlock);
	}

	uint64_t SnapshotTimeCC;	// FClockGenerator.ClockCounter
	std::vector<FDataBlock> DataBlocks;
};

class IMemory
{
public:
	IMemory() = default;
	virtual ~IMemory() = default;

	virtual void Snapshot(FMemorySnapshot& InOutMemorySnaphot, EMemoryOperationType Type) = 0;
	virtual void Load(const std::filesystem::path& FilePath) = 0;
	virtual void SetReadOnlyMode(bool bEnable = true) {};
};
