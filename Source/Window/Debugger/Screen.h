#pragma once

#include <CoreMinimal.h>
#include "Viewer.h"
#include "Utils/UI/Draw_ZXColorVideo.h"
#include "Devices/ControlUnit/Interface_Display.h"

class SScreen;
struct FSpectrumDisplay;

enum class EThreadStatus;

struct FScreenSettings
{
	uint32_t Width;
	uint32_t Height;

	FDisplayCycles DisplayCycles;
};

class SScreen : public SViewerChild, public IWindowEventNotification
{
	using Super = SViewerChild;
	using ThisClass = SScreen;
public:
	SScreen(EFont::Type _FontName);
	virtual ~SScreen() = default;

	virtual void Initialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Render() override;
	virtual void Destroy() override;

	// callback
	void OnDrawCallback(const ImDrawList* ParentList, const ImDrawCmd* CMD);

private:
	FORCEINLINE FMotherboard& GetMotherboard() const;

	void Load_MemoryScreen();

	void Draw_Display();

	void Input_HotKeys();
	void Input_Mouse();

	// events
	virtual void OnInputStep(FCPU_StepType Type) override;
	virtual void OnInputDebugger(bool bDebuggerState) override;

	// screen size
	FScreenSettings ScreenSettings;
	EThreadStatus Status;

	bool bDragging;
	std::shared_ptr<UI::FZXColorView> ZXColorView;
	std::shared_ptr<FSpectrumDisplay> SpectrumDisplay;
};
