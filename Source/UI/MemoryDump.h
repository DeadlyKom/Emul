#pragma once

#include <CoreMinimal.h>
#include "Window.h"

class SMemoryDump : public SWindow
{
public:
	SMemoryDump(EFont::Type _FontName);

	virtual void Initialize() override;
	virtual void Render() override;
};
