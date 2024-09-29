#pragma once

#include <CoreMinimal.h>
#include "Window.h"

class SCallStack : public SWindow
{
public:
	SCallStack(EFont::Type _FontName);

	virtual void Initialize() override;
	virtual void Render() override;
};