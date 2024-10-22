#include "DRAM.h"

#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME(Type) FName(std::format("{} {}", ThisDeviceName, ThisClass::ToString(Type)))

namespace
{
	static const char* ThisDeviceName = "DRAM";

	static const char* DRAM_4116_Name = "RAM 16kb\0";
	static const char* DRAM_4164_Name = "RAM 64kb\0";
	static const char* DRAM_41256_Name = "RAM 256kb\0";
	static const char* DRAM_41XXX_Name = "RAM XXXkb\0";
}

#define NotDelay

FDRAM::FDRAM(EDRAM_Type _Type, uint16_t _PlacementAddress, const std::vector<uint8_t>& _RawData, ESignalState::Type _WE)
	: FDevice(DEVICE_NAME(_Type), EName::DRAM, EDeviceType::Memory)
	, Type(_Type)
	, WriteEnable(_WE)
	, PlacementAddress(_PlacementAddress)
	, bRASLatch(false)
	, bCASLatch(false)
	, bRASCASLatch(false)
	, RowAddress(INDEX_NONE)
	, ColumnAddress(INDEX_NONE)
{
	switch (Type)
	{
		case EDRAM_Type::DRAM_4116:		// 16kb		(A0-A6)
		{
			RawData.resize((1 << 7 * 2));
			std::memcpy(RawData.data(), _RawData.data(), FMath::Min(RawData.size(), _RawData.size()));
			break;
		}
		case EDRAM_Type::DRAM_4164:		// 64kb		(A0-A7)
		{
			RawData.resize((1 << 8 * 2));
			std::memcpy(RawData.data(), _RawData.data(), FMath::Min(RawData.size(), _RawData.size()));
			break;
		}
		case EDRAM_Type::DRAM_41256:	// 256kb	(A0-A8)
		{
			RawData.resize((1 << 9 * 2));
			std::memcpy(RawData.data(), _RawData.data(), FMath::Min(RawData.size(), _RawData.size()));
			break;
		}
	}
}

std::string FDRAM::ToString(EDRAM_Type Type)
{
	switch (Type)
	{
		case EDRAM_Type::DRAM_4116: return DRAM_4116_Name;		// 16kb		(A0-A6)
		case EDRAM_Type::DRAM_4164: return DRAM_4164_Name;
		case EDRAM_Type::DRAM_41256: return DRAM_41256_Name;
		default: return DRAM_41XXX_Name;
	}
}

void FDRAM::Tick()
{
	const ESignalState::Type WriteEnable = SB->GetSignal(BUS_WE);
	if (bool bIsRAS = SB->IsNegativeEdge(BUS_RAS); !bRASLatch && bRASLatch != bIsRAS)
	{
		RowAddress = SB->GetDataOnMemAddressBus();
		bRASLatch = bIsRAS;
	}
	else if (bRASLatch && SB->IsPositiveEdge(BUS_RAS))
	{
		bRASLatch = false;
		bRASCASLatch = false;
	}
	else if (const bool bIsCAS = SB->IsNegativeEdge(BUS_CAS); !bCASLatch && bCASLatch != bIsCAS)
	{
		ColumnAddress = SB->GetDataOnMemAddressBus();
		bCASLatch = bIsCAS;
	}
	else if (bCASLatch && SB->IsPositiveEdge(BUS_CAS))
	{
		bCASLatch = false;
		bRASCASLatch = false;
	}

	// read/write, page read/write cycles
	if (!bRASCASLatch && bRASLatch && bCASLatch)
	{
		bRASCASLatch = true;
		uint32_t Address = ((ColumnAddress << 8) | RowAddress) - PlacementAddress;
		//switch (Type)
		//{
		//	case EDRAM_Type::DRAM_4116:		// 16kb		(A0-A6)
		//	{
		//		Address |= ColumnAddress << 7;
		//		Address -= PlacementAddress >> 1;
		//		break;
		//	}
		//	case EDRAM_Type::DRAM_4164:		// 64kb		(A0-A7)
		//	{
		//		Address |= ColumnAddress << 8;
		//		Address -= PlacementAddress;
		//		break;
		//	}
		//	case EDRAM_Type::DRAM_41256:	// 256kb	(A0-A8)
		//	{
		//		Address |= ColumnAddress << 9;
		//		Address -= PlacementAddress;
		//		break;
		//	}
		//}

		if (Address > RawData.size())
		{
			return;
		}

	#ifndef NotDelay
		ADD_EVENT_(CG, CG->ToNanosec(60), FrequencyDivider,
			[=]() -> void
			{
				if (WriteEnable == ESignalState::High) // read
				{
					const uint8_t Value = RawData[Address];
					SB->SetDataOnMemDataBus(Value);
				}
				else // write
				{
					const uint8_t Value = SB->GetDataOnMemDataBus();
					RawData[Address] = Value;
				}
			}, "Delay signal");
		#else
			if (WriteEnable == ESignalState::High) // read
			{
				const uint8_t Value = RawData[Address];
				SB->SetDataOnMemDataBus(Value);
			}
			else // write
			{
				const uint8_t Value = SB->GetDataOnMemDataBus();
				RawData[Address] = Value;
			}
		#endif
	}

	// RAS-Only refresh cycle
	else if (bRASLatch && !bCASLatch)
	{}
}

void FDRAM::Snapshot(FMemorySnapshot& InOutMemorySnaphot, EMemoryOperationType Type)
{
}

void FDRAM::Load(const std::filesystem::path& FilePath)
{
	if (!std::filesystem::exists(FilePath))
	{
		LOG("File does not exist: %s", FilePath.string().c_str());
		return;
	}

	std::ifstream File(FilePath, std::ios::in | std::ios::binary);
	if (!File.is_open())
	{
		LOG("Could not open the file: %s", FilePath.string().c_str());
		return;
	}

	// Stop eating new lines in binary mode
	File.unsetf(std::ios::skipws);

	// get its size
	std::streampos FileSize;
	File.seekg(0, std::ios::end);
	FileSize = File.tellg();
	File.seekg(0, std::ios::beg);

	// reserve capacity
	RawData.clear();
	RawData.resize(FileSize);

	// read the data
	RawData.insert(RawData.begin(), std::istream_iterator<BYTE>(File), std::istream_iterator<BYTE>());

	File.close();
}
