#pragma once

#include <CoreMinimal.h>
#include "Interface_Memory.h"
#include "Devices/Device.h"
#include "Utils/SignalsBus.h"

enum class EDRAM_Type
{
	DRAM_4116,			// 16kb		(A0-A6)
	DRAM_4164,			// 64kb		(A0-A7)
	DRAM_41256,			// 256kb	(A0-A8)
};

class FDRAM : public FDevice, public IMemory
{
	using ThisClass = FDRAM;
public:
	FDRAM(EDRAM_Type _Type, uint16_t _PlacementAddress, const std::vector<uint8_t>& _RawData = {}, ESignalState::Type _WE = ESignalState::HiZ);
	virtual ~FDRAM() = default;

	static std::string ToString(EDRAM_Type Type);
	virtual void Tick() override;
	virtual void Snapshot(FMemorySnapshot& InOutMemorySnaphot, EMemoryOperationType Type) override;
	virtual void Load(const std::filesystem::path& FilePath) override;

private:
	EDRAM_Type Type;
	ESignalState::Type WriteRead;

	uint16_t PlacementAddress;
	std::vector<uint8_t> RawData;
};
