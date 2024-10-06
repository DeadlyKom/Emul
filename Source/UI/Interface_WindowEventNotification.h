#pragma once

#include <CoreMinimal.h>

class IWindowEventNotification
{
public:
	virtual void OnInputDebugger(bool bDebuggerState) = 0;
};
