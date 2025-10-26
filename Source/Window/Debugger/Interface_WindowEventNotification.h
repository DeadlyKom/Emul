#pragma once

#include <CoreMinimal.h>

enum class FCPU_StepType;

enum class EEventNotificationType
{
	Input_Step,
	Input_Debugger,
	ForceUpdateStates,
};

class IWindowEventNotification
{
public:
	IWindowEventNotification() = default;
	virtual ~IWindowEventNotification() = default;

	virtual void OnInputStep(FCPU_StepType Type) {};
	virtual void OnInputDebugger(bool bDebuggerState) {};
	virtual void OnForceUpdateStates(bool bDebuggerState) {};
};
