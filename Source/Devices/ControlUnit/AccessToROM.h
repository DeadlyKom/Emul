#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"

class FAccessToROM : public FDevice
{
	using ThisClass = FAccessToROM;
public:
	FAccessToROM();

	virtual void Tick() override;
};
