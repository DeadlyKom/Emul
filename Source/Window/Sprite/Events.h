#pragma once

#include <Core/Event.h>
#include <Utils/UI/Draw_ZXColorVideo.h>

#include "Palette.h"
#include "ToolBar.h"

struct FSprite;

namespace FEventTag
{
	static const FName ChangeToolModeTag = TEXT("ChangeToolMode");
	static const FName CanvasOptionsFlagsTag = TEXT("CanvasOptionsFlags");
	static const FName ChangeColorTag = TEXT("ChangeColor");
	static const FName CanvasSizeTag = TEXT("CanvasSize");;
	static const FName MousePositionTag = TEXT("MouseState");
	static const FName AddSpriteTag = TEXT("AddSprite");
	static const FName SelectedSpritesChangedTag = TEXT("SelectedSpritesChanged");
}

struct FChangeToolMode
{
	EToolMode::Type ToolMode;
};

struct FEvent_Canvas : public IEvent
{
	uint32_t OptionsFlags{};
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

struct FEvent_Color : public IEvent
{
	uint8_t ButtonIndex = INDEX_NONE;											// pressed mouse button
	UI::EZXSpectrumColor::Type SelectedColorIndex = UI::EZXSpectrumColor::None;	// zx color
	ESubcolor::Type SelectedSubcolorIndex = ESubcolor::None;					// type ink/paper/bright
};

struct FEvent_Sprite : public IEvent
{
	// original image size
	int32_t Width;
	int32_t Height;
	ImRect SpriteRect;
	std::string SpriteName;
	std::filesystem::path SourcePathFile;

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

struct FEvent_SelectedSprite : public IEvent
{
	std::vector<std::shared_ptr<FSprite>> Sprites;
};
