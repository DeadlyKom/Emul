#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"

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
	FEPROM(EEPROM_Type _Type, const std::vector<uint8_t>& _Firmware);
	FEPROM(EEPROM_Type _Type, uint8_t* _Firmware = nullptr, uint32_t _FirmwareSize = 0);

	static std::string ToString(EEPROM_Type Type);
	virtual void Tick(FClockGenerator& CG, FSignalsBus& SB) override;

private:
	EEPROM_Type Type;
	std::vector<uint8_t> Firmware;
};
