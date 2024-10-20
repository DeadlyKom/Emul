#include "DRAM.h"

#define DEVICE_NAME(Type) FName(std::format("{} {}", ThisDeviceName, ThisClass::ToString(Type)))

namespace
{
	static const char* ThisDeviceName = "DRAM";

	static const char* DRAM_4116_Name = "RAM 16kb\0";
	static const char* DRAM_4164_Name = "RAM 64kb\0";
	static const char* DRAM_41256_Name = "RAM 256kb\0";
	static const char* DRAM_41XXX_Name = "RAM XXXkb\0";
}
FDRAM::FDRAM(EDRAM_Type _Type, uint16_t _PlacementAddress, const std::vector<uint8_t>& _RawData, ESignalState::Type _WE)
	: FDevice(DEVICE_NAME(_Type), EName::DRAM, EDeviceType::Memory)
	, Type(_Type)
	, WriteRead(_WE)
	, PlacementAddress(_PlacementAddress)
	, RawData(_RawData)
{}

std::string FDRAM::ToString(EDRAM_Type Type)
{
	switch (Type)
	{
		case EDRAM_Type::DRAM_4116: return DRAM_4116_Name;
		case EDRAM_Type::DRAM_4164: return DRAM_4164_Name;
		case EDRAM_Type::DRAM_41256: return DRAM_41256_Name;
		default: return DRAM_41XXX_Name;
	}
}

void FDRAM::Tick()
{
}

void FDRAM::Snapshot(FMemorySnapshot& InOutMemorySnaphot, EMemoryOperationType Type)
{
}

void FDRAM::Load(const std::filesystem::path& FilePath)
{
}
