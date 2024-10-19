#pragma once

#include <CoreMinimal.h>

struct FDisplayCycles
{
	uint32_t FlybackH;
	uint32_t BorderL;
	uint32_t DisplayH;
	uint32_t BorderR;

	uint32_t FlybackV;
	uint32_t BorderT;
	uint32_t DisplayV;
	uint32_t BorderB;
};

class IDisplay
{
public:
	IDisplay() = default;
	virtual ~IDisplay() = default;
	virtual void SetDisplayCycles(const FDisplayCycles& NewDisplayCycles) = 0;
};
