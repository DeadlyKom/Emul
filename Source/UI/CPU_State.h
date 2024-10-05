#pragma once

#include <CoreMinimal.h>
#include "Utils/UI.h"
#include "Viewer.h"
#include "Devices/CPU/Z80.h"

class FMotherboard;
enum class EThreadStatus;

struct FRegisterVisual
{
	const char* Name;
	uint8_t* Value;
	int32_t H_Color;
	int32_t L_Color;
	bool bWord;
};

class SCPU_State : public SViewerChild
{
	using Super = SViewerChild;
	using ThisClass = SCPU_State;
public:
	SCPU_State(EFont::Type _FontName);

	virtual void Initialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Render() override;

	const FRegisters& GetLatestRegistersState() const { return LatestRegistersState; }

private:
	FORCEINLINE FMotherboard& GetMotherboard() const;

	void Update_Registers();

	void Draw_States(bool bEnabled);
	void Draw_Registers();
	void Draw_Registers_Row(const std::vector<FRegisterVisual>& RowVisual);

	void Input_HotKeys();

	uint64_t LatestClockCounter;
	EThreadStatus Status;
	FRegisters LatestRegistersState;
	std::vector<std::vector<FRegisterVisual>> HighlightRegisters;
};
