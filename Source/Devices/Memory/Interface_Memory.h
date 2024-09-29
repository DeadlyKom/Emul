#pragma once

#include <CoreMinimal.h>

enum class EDataBlockState
{
	Unknown,
	Actived,
};

struct FDataBlock
{
	FName DeviceName;
	FName BlockName;
	EDataBlockState State;
	uint32_t PlacementAddress;
	std::vector<uint8_t> Data;
};

struct FMemorySnapshot
{
	FMemorySnapshot(uint64_t SnapshotTime = -1)
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
	virtual void Snapshot(FMemorySnapshot& OutMemorySnaphot) = 0;
	virtual void Load(const std::filesystem::path& FilePath) = 0;
};
