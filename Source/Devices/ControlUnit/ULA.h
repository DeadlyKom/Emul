#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"
#include "Interface_Display.h"

class FULA : public FDevice, public IDisplay
{
	using ThisClass = FULA;
public:
	FULA(const FDisplayCycles& _DisplayCycles, double _Frequency);
	virtual ~FULA() = default;

	virtual void Tick() override;
	virtual void CalculateFrequency(double MainFrequency) override;
	virtual void SetDisplayCycles(const FDisplayCycles& NewDisplayCycles);

private:
	uint32_t Scanline;
	uint32_t Line;
	uint32_t Frame;
	uint32_t DisplayWidth;
	uint32_t DisplayHeight;

	FDisplayCycles DisplayCycles;
	std::vector<uint8_t> Display;
};
