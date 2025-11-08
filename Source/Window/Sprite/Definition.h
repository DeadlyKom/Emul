#pragma once

#include <CoreMinimal.h>

#define SPRITE_BUILD_MAJOR 0
#define SPRITE_BUILD_MINOR 1

struct FRGBAImage
{
	int32_t Width;
	int32_t Height;
	std::vector<uint8_t> Data;

	FRGBAImage(int32_t _Width = INDEX_NONE, int32_t _Height = INDEX_NONE)
		: Width(_Width)
		, Height(_Height)
	{}

	bool IsValid() const
	{
		return Width != INDEX_NONE && Height != INDEX_NONE && Data.size() > 1;
	}
};

namespace Window
{
	bool ClipboardData(FRGBAImage& OutputRGBAImage);
}
