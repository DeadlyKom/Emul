#pragma once

#include <CoreMinimal.h>
#include "Devices/Memory/Interface_Memory.h"

namespace Memory
{
	void ToAddressSpace(const FMemorySnapshot& Snapshot, std::vector<uint8_t>& OutRawData, uint32_t MemoryAreaSize = 65536)
	{
		OutRawData.resize(MemoryAreaSize);
		for (const FDataBlock& DataBlock : Snapshot.DataBlocks)
		{
			memcpy(OutRawData.data() + DataBlock.PlacementAddress, DataBlock.Data.data(), DataBlock.Data.size());
		}
	}
}
