#pragma once

#include <CoreMinimal.h>
#include "Devices/Memory/Interface_Memory.h"

namespace Memory
{
	static inline void ToAddressSpace(const FMemorySnapshot& Snapshot, std::vector<uint8_t>& OutRawData, uint32_t MemoryAreaSize = 65536)
	{
		OutRawData.resize(MemoryAreaSize);
		for (const FDataBlock& DataBlock : Snapshot.DataBlocks)
		{
			const size_t CopySize = Math::Min(DataBlock.Data.size(), OutRawData.size() - DataBlock.PlacementAddress);
			std::ranges::copy(DataBlock.Data.begin(), DataBlock.Data.begin() + CopySize, OutRawData.begin() + DataBlock.PlacementAddress);
		}
	}

	static inline void ToSnapshot(FMemorySnapshot& Snapshot, const std::vector<uint8_t>& InRawData, uint32_t MemoryAreaSize = 65536)
	{
		for (FDataBlock& DataBlock : Snapshot.DataBlocks)
		{
			std::ranges::copy(InRawData.begin() + DataBlock.PlacementAddress, InRawData.begin() + DataBlock.Data.size(), DataBlock.Data.begin());
		}
	}

	static inline bool GetNameByAddress(FMemorySnapshot& Snapshot, uint32_t Address, std::string& Output)
	{
		for (FDataBlock& DataBlock : Snapshot.DataBlocks)
		{
			if (DataBlock.PlacementAddress <= Address && DataBlock.PlacementAddress + DataBlock.Data.size() > Address)
			{
				Output = DataBlock.BlockName.ToString();
				return true;
			}
		}
		return  false;
	}

	static inline bool GetAccessMode(FMemorySnapshot& Snapshot, uint32_t Address, bool& Output)
	{
		for (FDataBlock& DataBlock : Snapshot.DataBlocks)
		{
			if (DataBlock.PlacementAddress <= Address && DataBlock.PlacementAddress + DataBlock.Data.size() > Address)
			{
				Output = DataBlock.bReadOnlyMode;
				return true;
			}
		}
		return  false;
	}
	
	static inline bool SetAccessMode(FMemorySnapshot& Snapshot, uint32_t Address, bool NewAccessMode)
	{
		for (FDataBlock& DataBlock : Snapshot.DataBlocks)
		{
			if (DataBlock.PlacementAddress <= Address && DataBlock.PlacementAddress + DataBlock.Data.size() > Address)
			{
				DataBlock.bReadOnlyMode = NewAccessMode;
				return true;
			}
		}
		return  false;
	}
}
