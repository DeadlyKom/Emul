#include "EPROM.h"
#include "AppFramework.h"
#include "Devices/Device.h"
#include "Utils/SignalsBus.h"
#include "Devices/ControlUnit/AccessToROM.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

#define DEVICE_NAME(Type) FName(std::format("{} {}", ThisDeviceName, ThisClass::ToString(Type)))

namespace
{
	static const char* ThisDeviceName = "EPROM";

	static const char* EPROM_27C128_Name = "27C128\0";
	static const char* EPROM_27C256_Name = "27C256\0";
	static const char* EPROM_27C512_Name = "27C512\0";
	static const char* EPROM_27C010_Name = "27C010\0";
	static const char* EPROM_27C020_Name = "27C020\0";
	static const char* EPROM_27C040_Name = "27C040\0";
	static const char* EPROM_27CXXX_Name = "27CXXX\0";
}

FEPROM::FEPROM(EEPROM_Type _Type,
			   uint16_t _PlacementAddress,
			   const std::vector<uint8_t>& _Firmware,
			   ESignalState::Type _CE /*= ESignalState::High*/,
			   ESignalState::Type _OE /*= ESignalState::High*/)
	: FDevice(DEVICE_NAME(_Type), EName::EPROM, EDeviceType::Memory)
	, Type(_Type)
	, ChipEnable(_CE)
	, OutputEnable(_OE)
	, PlacementAddress(_PlacementAddress)
	, Firmware(_Firmware)
{}

FEPROM::FEPROM(EEPROM_Type _Type,
			   uint16_t _PlacementAddress,
			   uint8_t* _Firmware /*= nullptr*/,
			   uint32_t _FirmwareSize /*= 0*/,
			   ESignalState::Type _CE /*= ESignalState::High*/,
			   ESignalState::Type _OE /*= ESignalState::High*/)
	: FDevice(FName(DEVICE_NAME(_Type)), EName::EPROM, EDeviceType::Memory)
	, Type(_Type)
	, ChipEnable(_CE)
	, OutputEnable(_OE)
	, PlacementAddress(_PlacementAddress)
{
	if (_Firmware != 0 && _FirmwareSize != 0)
	{
		std::copy(_Firmware, _Firmware + _FirmwareSize, Firmware.begin());
	}
}

std::string FEPROM::ToString(EEPROM_Type Type)
{
	switch (Type)
	{
	case EEPROM_Type::EPROM_27C128: return EPROM_27C128_Name;
	case EEPROM_Type::EPROM_27C256: return EPROM_27C256_Name;
	case EEPROM_Type::EPROM_27C512: return EPROM_27C512_Name;
	case EEPROM_Type::EPROM_27C010: return EPROM_27C010_Name;
	case EEPROM_Type::EPROM_27C020: return EPROM_27C020_Name;
	case EEPROM_Type::EPROM_27C040: return EPROM_27C040_Name;
	default: return EPROM_27CXXX_Name;
	}
}

void FEPROM::Tick()
{
	OutputEnable = SB->GetSignal(BUS_RD_ROM);

	if (ChipEnable   != ESignalState::Low || 
		OutputEnable != ESignalState::Low)
	{
		return;
	}

	const uint16_t Address = SB->GetDataOnAddressBus();
	if (Address < Firmware.size())
	{
		const uint8_t Value = Firmware[Address];
		ADD_EVENT_(CG, CG->ToNanosec(60), "Delay signal",
			[=]() -> void
			{
				SB->SetDataOnDataBus(Value);
			});
	}
	else
	{
		// unstable data bus
		const uint8_t Value = rand();
		ADD_EVENT_(CG, CG->ToNanosec(60), "Delay signal",
			[=]() -> void
			{
				SB->SetDataOnDataBus(Value);
			});
	}
}

void FEPROM::Snapshot(FMemorySnapshot& InOutMemorySnaphot, EMemoryOperationType Type)
{
	if (Type == EMemoryOperationType::Read)
	{
		FDataBlock DataBlock
		{
			.DeviceName = DeviceName,
			.BlockName = "Test ROM",
			.State = EDataBlockState::Actived,
			.PlacementAddress = PlacementAddress,
			.Data = Firmware,
		};
		InOutMemorySnaphot.AddDataBlock(DataBlock);
	}
	else if (Type == EMemoryOperationType::Write)
	{
		for (FDataBlock& DataBlock : InOutMemorySnaphot.DataBlocks)
		{
			if (DataBlock.DeviceName != DeviceName)
			{
				continue;
			}
			std::ranges::copy(DataBlock.Data.begin(), DataBlock.Data.end(), Firmware.begin() + DataBlock.PlacementAddress);
		}
	}
}

void FEPROM::Load(const std::filesystem::path& FilePath)
{
	if (!std::filesystem::exists(FilePath))
	{
		LOG_CONSOLE("File does not exist: %s", FilePath.string().c_str());
		return;
	}

	std::ifstream File(FilePath, std::ios::in | std::ios::binary);
	if (!File.is_open())
	{
		LOG_CONSOLE("Could not open the file: %s", FilePath.string().c_str());
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
	Firmware.clear();
	Firmware.reserve(FileSize);

	// read the data
	Firmware.insert(Firmware.begin(), std::istream_iterator<BYTE>(File), std::istream_iterator<BYTE>());

	File.close();
}
