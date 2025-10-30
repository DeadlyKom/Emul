#pragma once

#include <Core/Event.h>
#include <Utils/UI/Draw_ZXColorVideo.h>

#include "Palette.h"
#include "ToolBar.h"

namespace FEventTag
{
	static const FName ChangeToolModeTag = TEXT("ChangeToolMode");
	static const FName CanvasOptionsFlagsTag = TEXT("CanvasOptionsFlags");
	static const FName SelectedColorTag = TEXT("SelectedColor");
	static const FName CanvasSizeTag = TEXT("CanvasSize");;
	static const FName MousePositionTag = TEXT("MouseState");
	static const FName AddSpriteTag = TEXT("AddSprite");
}

struct FSelectedColor
{
	uint8_t ButtonIndex{};
	UI::EZXSpectrumColor::Type SelectedColorIndex = UI::EZXSpectrumColor::None;
	ESubcolor::Type SelectedSubcolorIndex = ESubcolor::None;
};

struct FChangeToolMode
{
	EToolMode::Type ToolMode;
};

struct FEvent_Canvas : public IEvent
{
	uint32_t OptionsFlags{};
	FSelectedColor SelectedColor;
	FChangeToolMode ChangeToolMode;
};

struct FEvent_StatusBar : public IEvent
{
	ImVec2 CanvasSize{};
	ImVec2 MousePosition{};
};

struct FEvent_ToolBar : public IEvent
{
	FChangeToolMode ChangeToolMode;
};

struct FEvent_PaletteBar : public IEvent
{
	FSelectedColor SelectedColor;
};

//
struct FEvent_AddSprite : public IEvent
{
	// original image size
	int32_t Width;
	int32_t Height;
	ImRect SpriteRect;
	std::string SpriteName;

	// ZX Spectrum viewing data
	std::vector<uint8_t> IndexedData;	// indexed image data after QuantizeToZX
	std::vector<uint8_t> InkData;		// pixels
	std::vector<uint8_t> AttributeData;	// color ink and color paper (fbpppiii)
	// f - flag, flash (swap color inc and paper)
	// b - flag, bright colors
	// p - 3-bit color paper
	// i - 3-bit color pixel
	std::vector<uint8_t> MaskData;		// auto mask from alpha channel
};