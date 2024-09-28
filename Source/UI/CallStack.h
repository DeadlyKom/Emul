#pragma once

#include <CoreMinimal.h>
#include "Window.h"

class SCallStack : public SWindow
{
public:
	SCallStack();

	virtual void Initialize() override;
	virtual void Render() override;
};
