#pragma once

#include <CoreMinimal.h>
#include "Viewer.h"
#include "Core/Image.h"
#include "Devices/ControlUnit/Interface_Display.h"

class SScreen;
struct FSpectrumDisplay;

enum class EThreadStatus;

enum class ERenderType
{
	Unknown,
	Screen,
};

struct FRenderData
{
	FRenderData(std::shared_ptr<SScreen> _Screen = nullptr, ERenderType _RenderType = ERenderType::Unknown)
		: Screen(_Screen)
		, RenderType(_RenderType)
	{}

	std::shared_ptr<SScreen> Screen;
	ERenderType RenderType;
};

struct FScreenSettings
{
	bool bAttributeGrid = false;
	bool bGrid = false;
	bool bPixelGrid = true;
	ImVec2 GridSettingSize = ImVec2(8.0f, 8.0f);
	ImVec2 GridSettingOffset = ImVec2(0.0f, 0.0f);

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

	void ConvertDisplayDataToRGB(const FSpectrumDisplay& DS);
	void RoundImagePosition();
	ImVec2 CalculatePanelSize();
	void SetScale(float scaleY);
	void SetScale(ImVec2 NewScale);
	void SetImagePosition(ImVec2 NewPosition);
	ImVec2 ConverPositionToPixel(const ImVec2& Position);
	Transform2D GetTexelsToPixels(const ImVec2& ScreenTopLeft, const ImVec2& ScreenViewSize, const ImVec2& UVTopLeft, const ImVec2& UVViewSize, const ImVec2& TextureSize);

	// screen size
	FScreenSettings ScreenSettings;

	// scale
	float ZoomRate;
	float PixelAspectRatio;
	float MinimumGridSize;
	ImVec2 Scale;
	ImVec2 OldScale;
	ImVec2 ScaleMin;
	ImVec2 ScaleMax;

	// view state	
	ImVec2 ImagePosition;
	ImVec2 PanelTopLeftPixel;
	ImVec2 PanelSize;
	ImVec2 ViewTopLeftPixel;
	ImVec2 ViewSize;
	ImVec2 ViewSizeUV;
	Transform2D TexelsToPixels;
	Transform2D PixelsToTexels;

	// texture
	ImRect UV;
	ImVec2 TextureSizePixels;
	
	// shader
	ID3D11Buffer* PSCB_Grid;
	ID3D11PixelShader* PS_Grid;

	// shader variable
	float TimeCounter;
	bool bBeamEnable;
	bool bForceNearestSampling;
	ImVec2 GridWidth;
	ImVec2 GridSize;
	ImVec2 GridOffset;
	ImVec4 GridColor;
	ImVec4 BackgroundColor;

	// render data
	std::shared_ptr<FRenderData> SpectrumScreen;

	EThreadStatus Status;

	bool bDragging;
	FImage Image;
	FSpectrumDisplay SpectrumDisplay;
	std::vector<uint32_t> DisplayRGBA;
};
