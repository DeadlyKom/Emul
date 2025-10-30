#pragma once

#include <CoreMinimal.h>
#include <Utils/UI/Draw.h>
#include "Viewer.h"
#include "Devices/CPU/Interface_CPU_Z80.h"

class FMotherboard;
enum class EThreadStatus;

enum class ERegisterVisualType : uint8_t
{
	NONE,
	BOOL,
	BYTE,
	WORD,
	INT,
};
struct FRegisterVisual
{
	const char* Name = nullptr;
	uint8_t* Value = nullptr;
	int32_t H_Color;
	int32_t L_Color;
	ERegisterVisualType ValueType = ERegisterVisualType::NONE;
	uint8_t NewValue = (uint8_t)INDEX_NONE;
};

class SCPU_State : public SViewerChild
{
	using Super = SViewerChild;
	using ThisClass = SCPU_State;
public:
	SCPU_State(EFont::Type _FontName);
	virtual ~SCPU_State() = default;

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

	uint8_t LatestFlags, NewFlags;
	uint64_t LatestClockCounter;
	EThreadStatus Status;
	FRegisters LatestRegistersState;
	std::vector<std::vector<FRegisterVisual>> HighlightRegisters;
};
