#include "EPROM.h"
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
			   const std::vector<uint8_t>& _Firmware,
			   ESignalState::Type _CE /*= ESignalState::High*/,
			   ESignalState::Type _OE /*= ESignalState::High*/)
	: FDevice(DEVICE_NAME(_Type), EName::EPROM, EDeviceType::Memory)
	, Type(_Type)
	, ChipEnable(_CE)
	, OutputEnable(_OE)
	, Firmware(_Firmware)
{}

FEPROM::FEPROM(EEPROM_Type _Type,
			   uint8_t* _Firmware /*= nullptr*/,
			   uint32_t _FirmwareSize /*= 0*/,
			   ESignalState::Type _CE /*= ESignalState::High*/,
			   ESignalState::Type _OE /*= ESignalState::High*/)
	: FDevice(FName(DEVICE_NAME(_Type)), EName::EPROM, EDeviceType::Memory)
	, ChipEnable(_CE)
	, OutputEnable(_OE)
	, Type(_Type)
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
