#pragma once

#include <CoreMinimal.h>
#include "Interface_Memory.h"
#include "Devices/Device.h"
#include "Utils/Signal/Bus.h"

enum class EEPROM_Type
{
	EPROM_27C128,		// 16kb
	EPROM_27C256,		// 32kb
	EPROM_27C512,		// 64kb
	EPROM_27C010,		// 128kb
	EPROM_27C020,		// 256kb
	EPROM_27C040,		// 512kb
};

class FEPROM : public FDevice, public IMemory
{
	using ThisClass = FEPROM;
public:
	FEPROM(EEPROM_Type _Type, uint16_t _PlacementAddress, const std::vector<uint8_t>& _Firmware, ESignalState::Type _CE = ESignalState::High, ESignalState::Type _OE = ESignalState::High);
	FEPROM(EEPROM_Type _Type, uint16_t _PlacementAddress, uint8_t* _Firmware = nullptr, uint32_t _FirmwareSize = 0, ESignalState::Type _CE = ESignalState::High, ESignalState::Type _OE = ESignalState::High);
	virtual ~FEPROM() = default;

	static std::string ToString(EEPROM_Type Type);
	virtual void Tick() override;
	virtual void Snapshot(FMemorySnapshot& InOutMemorySnaphot, EMemoryOperationType Type) override;
	virtual void Load(const std::filesystem::path& FilePath) override;
	virtual void SetReadOnlyMode(bool bEnable = true) override;

private:
	EEPROM_Type Type;
	ESignalState::Type ChipEnable;
	ESignalState::Type OutputEnable;
	bool bReadOnlyMode;
	uint16_t PlacementAddress;
	std::vector<uint8_t> Firmware;
};
