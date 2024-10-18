#pragma once

#include <CoreMinimal.h>

class IWindowEventNotification
{
public:
	IWindowEventNotification() = default;
	virtual ~IWindowEventNotification() = default;

	virtual void OnInputDebugger(bool bDebuggerState) = 0;
};
