#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"
#include "Utils/SignalsBus.h"

enum class EEPROM_Type
{
	EPROM_27C128,		// 16kb
	EPROM_27C256,		// 32kb
	EPROM_27C512,		// 64kb
	EPROM_27C010,		// 128kb
	EPROM_27C020,		// 256kb
	EPROM_27C040,		// 512kb
};

class FEPROM : public FDevice
{
	using ThisClass = FEPROM;
public:
	FEPROM(EEPROM_Type _Type, const std::vector<uint8_t>& _Firmware, ESignalState::Type _CE = ESignalState::High, ESignalState::Type _OE = ESignalState::High);
	FEPROM(EEPROM_Type _Type, uint8_t* _Firmware = nullptr, uint32_t _FirmwareSize = 0, ESignalState::Type _CE = ESignalState::High, ESignalState::Type _OE = ESignalState::High);

	static std::string ToString(EEPROM_Type Type);
	virtual void Tick(FClockGenerator& CG, FSignalsBus& SB) override;

private:
	EEPROM_Type Type;
	ESignalState::Type ChipEnable;
	ESignalState::Type OutputEnable;
	std::vector<uint8_t> Firmware;
};
