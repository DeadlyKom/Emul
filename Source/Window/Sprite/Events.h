#pragma once

#include <Core/Event.h>
#include "Palette.h"
#include "Utils/UI/Draw_ZXColorVideo.h"

class FEvent_Canvas : public IEvent
{
public:
	static const FName CanvasOptionsFlagsTag;

	uint32_t OptionsFlags{};
};

class FEvent_StatusBar : public IEvent
{
public:
	static const FName CanvasSizeTag;
	static const FName MousePositionTag;

	ImVec2 CanvasSize{};
	ImVec2 MousePosition{};
};

class FEvent_PaletteBar : public IEvent
{
public:
	static const FName ColorIndexTag;

	uint8_t ButtonIndex{};
	UI::EZXSpectrumColor::Type SelectedColorIndex = UI::EZXSpectrumColor::None;
	ESubcolor::Type SelectedSubColorIndex = ESubcolor::None;
};