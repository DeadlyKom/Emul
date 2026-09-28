#pragma once

#include "Palette.h"

struct FPixelToCanvas
{
	std::vector<ImVec2> Position;
	std::vector<uint32_t> Color;
	uint32_t Canvas;
	uint8_t Subcolor[ESubcolor::MAX] = {};
	EPixelOperation::Type PixelOperation = EPixelOperation::Set;
};
