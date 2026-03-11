#pragma once

#include <Core/Event.h>
#include <Utils/UI/Draw_ZXColorVideo.h>

#include "Palette.h"
#include "ToolBar.h"
#include "AppSprite.h"

struct FSprite;

namespace FEventTag
{
	static const FName ChangeToolModeTag = TEXT("ChangeToolMode");
	static const FName RequestToolModeTag = TEXT("RequestToolMode");
	static const FName CanvasOptionsFlagsTag = TEXT("CanvasOptionsFlags");
	static const FName CanvasViewFlagsTag = TEXT("CanvasViewFlags");
	static const FName CanvasViewScaleTag = TEXT("CanvasViewScale");
	static const FName CanvasViewPositionTag = TEXT("CanvasViewPosition");
	static const FName ChangeColorTag = TEXT("ChangeColor");
	static const FName CanvasSizeTag = TEXT("CanvasSize");;
	static const FName MousePositionTag = TEXT("MouseState");
	static const FName AddSpriteTag = TEXT("AddSprite");
	static const FName AddedSpriteTag = TEXT("AddedSprite");
	static const FName RenamedSpriteTag = TEXT("RenamedSprite");
	static const FName UpdateSpriteTag = TEXT("UpdateSprite");
	static const FName SelectedSpritesChangedTag = TEXT("SelectedSpritesChanged");

	static const FName RequestCanvasViewFlagsTag = TEXT("RequestCanvasViewFlags");
	static const FName RequestAllSpritesTag = TEXT("RequestAllSprites");
	static const FName ResponseAllSpritesTag = TEXT("ResponseAllSprites");
}

struct FChangeToolMode
{
	EToolMode::Type ToolMode;
};

struct FEvent_Canvas : public IEvent
{
	std::wstring CanvasName;
	uint32_t OptionsFlags;
	FChangeToolMode ChangeToolMode;

	int32_t CanvasWidth;
	int32_t CanvasHeight;

	float MouseWheel;
	ImVec2 ImagePosition;

	FViewFlags ViewFlags;

	using IEvent::IEvent;
};

struct FEvent_StatusBar : public IEvent
{
	ImVec2 CanvasSize{};
	ImVec2 MousePosition{};
};

struct FEvent_ToolBar : public IEvent
{
	FChangeToolMode ChangeToolMode;
	using IEvent::IEvent;
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

	// auxiliary variables
	int32_t AsepriteIndex;
	int32_t UniqueID;

	int32_t CanvasWidth;
	int32_t CanvasHeight;

	using IEvent::IEvent;
};

struct FEvent_SelectedSprite : public IEvent
{
	std::shared_ptr<FSprite> Sprite;
};

struct FEvent_ImportJSON : public IEvent
{
	std::filesystem::path FilePath;
};
