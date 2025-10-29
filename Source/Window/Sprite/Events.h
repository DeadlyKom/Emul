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

class FEvent_Canvas : public IEvent
{
public:
	uint32_t OptionsFlags{};
	FSelectedColor SelectedColor;
	FChangeToolMode ChangeToolMode;
};

class FEvent_StatusBar : public IEvent
{
public:

	ImVec2 CanvasSize{};
	ImVec2 MousePosition{};
};

class FEvent_ToolBar : public IEvent
{
public:
	FChangeToolMode ChangeToolMode;
};

class FEvent_PaletteBar : public IEvent
{
public:
	FSelectedColor SelectedColor;
};