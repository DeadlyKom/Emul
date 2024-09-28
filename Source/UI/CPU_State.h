#pragma once

#include <CoreMinimal.h>
#include "Utils/UI.h"
#include "Window.h"
#include "Devices/CPU/Z80.h"

class FMotherboard;

class SCPU_State : public SWindow
{
	using ThisClass = SCPU_State;
public:
	SCPU_State();

	virtual void Initialize() override;
	virtual void Render() override;

private:
	FMotherboard& GetMotherboard() const;

	void Update_Registers();

	void DrawStates(bool bEnabled);
	void Column_DrawRegisters();

	void Input_HotKeys();

	FRegisters LatestRegistersState;

	// highlight registers with color
	const ImVec4* RegisterColor[FRegisters::PrimaryRegistrars];
};
