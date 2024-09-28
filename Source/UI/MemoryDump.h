#pragma once

#include <CoreMinimal.h>
#include "Window.h"

class SMemoryDump : public SWindow
{
public:
	SMemoryDump();

	virtual void Initialize() override;
	virtual void Render() override;
};
