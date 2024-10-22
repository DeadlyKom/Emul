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
	virtual bool MainTick() override;
	virtual void CalculateFrequency(double MainFrequency, uint32_t Sampling) override;
	virtual void SetDisplayCycles(const FDisplayCycles& NewDisplayCycles) override;
	virtual void GetSpectrumDisplay(FSpectrumDisplay& OutputDisplay) const override;

private:
	void BusLogic(uint32_t FrameClock);
	void ULALogic(uint32_t FrameClock);
	void VideoFetch();
	void DrawLogic(uint32_t FrameClock);
	
	uint32_t Scanline;
	uint32_t Line;
	uint32_t Frame;
	uint32_t DisplayWidth;
	uint32_t DisplayHeight;

	bool bIsBorder;
	bool bBorderDelay;
	bool bIsVideoFetch;
	bool bIsFlyback;
	bool bDrawPixels;
	bool bFlipFlopFlash;
	bool bIsInterrupt;
	bool bInterruptLatch;
	uint32_t Y;
	uint32_t X;
	uint8_t FlashCounter;
	//uint8_t PixelCounter;
	uint16_t Pixels;
	uint8_t PixelsShift;
	uint16_t Attribute;
	uint8_t AttributeLatch;

	FDisplayCycles DisplayCycles;
	std::vector<uint8_t> DisplayData;
};
