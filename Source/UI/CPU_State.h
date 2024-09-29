#pragma once

#include <CoreMinimal.h>
#include "Utils/UI.h"
#include "Window.h"
#include "Devices/CPU/Z80.h"

class FMotherboard;

struct FRegisterVisual
{
	const char* Name;
	uint8_t* Value;
	int32_t H_Color;
	int32_t L_Color;
	bool bWord;
};

class SCPU_State : public SWindow
{
	using ThisClass = SCPU_State;
public:
	SCPU_State(EFont::Type _FontName);

	virtual void Initialize() override;
	virtual void Render() override;

private:
	FORCEINLINE FMotherboard& GetMotherboard() const;

	void Update_Registers();

	void Draw_States(bool bEnabled);
	void Draw_Registers();
	void Draw_Registers_Row(const std::vector<FRegisterVisual>& RowVisual);

	void Input_HotKeys();

	uint64_t LatestClockCounter;
	FRegisters LatestRegistersState;
	std::vector<std::vector<FRegisterVisual>> HighlightRegisters;
};
