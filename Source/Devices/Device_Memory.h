#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"

class FMemory : public FDevice
{
public:
	FMemory();

	virtual void Tick(FClockGenerator& CG, uint64_t& InOutISB) override;
};
