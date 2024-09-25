#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"

class FAccessToROM : public FDevice
{
	using ThisClass = FAccessToROM;
public:
	FAccessToROM();

	virtual void Tick(FClockGenerator& CG, FSignalsBus& SB) override;

	static const FName SignalName_RD_ROM;

private:
	virtual void Register(FSignalsBus& SB);
};
