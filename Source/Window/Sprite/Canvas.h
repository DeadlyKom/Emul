#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include "Core/Image.h"
#include <Utils/UI/Draw_ZXColorVideo.h>
#include "Palette.h"
#include "ToolBar.h"

namespace FCanvasOptionsFlags
{
	enum Type
	{
		None = 0,
		Source = 1 << 0,
		Ink = 1 << 1,
		Attribute = 1 << 2,
		Mask = 1 << 3,
	};
}

class SCanvas : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SCanvas;
public:
	SCanvas(EFont::Type _FontName);
	virtual ~SCanvas() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize() override;
	virtual void Render() override;
	virtual void Destroy() override;

private:
	void Input_HotKeys();
	void Input_Mouse();

	// set mode
	void Imput_SetToolMode_None()				{ ToolMode[0] = EToolMode::None;				ToolMode[1] = EToolMode::None; }
	void Imput_SetToolMode_RectangleMarquee()	{ ToolMode[0] = EToolMode::RectangleMarquee;	ToolMode[1] = EToolMode::None; }
	void Imput_SetToolMode_Pencil()				{ ToolMode[0] = EToolMode::Pencil;				ToolMode[1] = EToolMode::None; }
	void Imput_SetToolMode_Eraser()				{ ToolMode[0] = EToolMode::Eraser;				ToolMode[1] = EToolMode::None; }
	void Imput_SetToolMode_Eyedropper()			{ ToolMode[0] = EToolMode::Eyedropper;			ToolMode[1] = EToolMode::None; }
	void Imput_SetToolMode_PaintBucket()		{ ToolMode[0] = EToolMode::PaintBucket;			ToolMode[1] = EToolMode::None; }
	void ApplyToolMode();

	void Handler_Pencil();
	void Handler_Eyedropper();

	void ConversionToZX(const UI::FConversationSettings& Settings);
	void ConversionToCanvas(const UI::FConversationSettings& Settings);
	void Set_PixelToCanvas(const ImVec2& Position, uint8_t ButtonIndex);

	void UpdateCursorColor(bool bButton = false);

	bool bDragging;
	bool bRefreshCanvas;
	bool bNeedConvertCanvasToZX;
	bool bNeedConvertZXToCanvas;
	ImGuiID CanvasID;

	// draw pixels
	UI::EZXSpectrumColor::Type ButtonColor[2];
	uint8_t Subcolor[ESubcolor::MAX];
	uint8_t LastSetButtonIndex;
	uint8_t LastSetPixelColorIndex;
	ImVec2 LastSetPixelPosition;

	int32_t Width;
	int32_t Height;
	uint32_t OptionsFlags[2];
	uint32_t LastOptionsFlags;
	std::shared_ptr<UI::FZXColorView> ZXColorView;
	UI::FConversationSettings ConversationSettings;
	EToolMode::Type ToolMode[2];
};
