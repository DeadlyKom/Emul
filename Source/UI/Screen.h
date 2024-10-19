#pragma once

#include <CoreMinimal.h>
#include "Viewer.h"
#include "Core/Image.h"

class SScreen;

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

	// display time cycle
	uint32_t FlybackH;
	uint32_t BorderL;
	uint32_t DisplayH;
	uint32_t BorderR;

	uint32_t FlybackV;
	uint32_t BorderT;
	uint32_t DisplayV;
	uint32_t BorderB;

	uint32_t Width;
	uint32_t Height;
};

class SScreen : public SViewerChild
{
	using Super = SViewerChild;
	using ThisClass = SScreen;
public:
	SScreen(EFont::Type _FontName);
	virtual ~SScreen() = default;

	virtual void Initialize() override;
	virtual void Render() override;
	virtual void Destroy() override;

	// callback
	void OnDrawCallback(const ImDrawList* ParentList, const ImDrawCmd* CMD);

private:
	void Draw_Display();

	void Input_HotKeys();
	void Input_Mouse();

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
	bool bForceNearestSampling;
	ImVec2 GridWidth;
	ImVec2 GridSize;
	ImVec2 GridOffset;
	ImVec4 GridColor;
	ImVec4 BackgroundColor;

	// render data
	std::shared_ptr<FRenderData> SpectrumScreen;

	bool bDragging;
	FImage Image;
	std::vector<uint32_t> DisplayRGBA;
};
