#pragma once

#include <CoreMinimal.h>

namespace AsepriteFormat
{
	enum class EColorMode
	{
		RGB,
		GRAYSCALE,
		INDEXED,
		BITMAP,
		TILEMAP,
	};

	inline constexpr int32_t BytesPerPixelForColormode(EColorMode CM)
	{
		switch (CM)
		{
		case EColorMode::RGB:       return 4;	// RgbTraits::bytes_per_pixel
		case EColorMode::GRAYSCALE: return 2;	// GrayscaleTraits::bytes_per_pixel
		case EColorMode::INDEXED:   return 1;	// IndexedTraits::bytes_per_pixel
		case EColorMode::BITMAP:    return 1;	// BitmapTraits::bytes_per_pixel
		case EColorMode::TILEMAP:   return 4;   // TilemapTraits::bytes_per_pixel
		}
		return 0;
	}

	struct FFrame
	{
		int32_t Width;
		int32_t Height;
		std::vector<uint8_t> Image;
		uint32_t TransparentColor;
	};

	struct FSprite
	{
		EColorMode ColorMode;
		int32_t Width;
		int32_t Height;
		uint32_t TransparentColor;

		std::vector<std::vector<uint8_t>> Frames;
		std::vector<int32_t> DurationPerFrame;
	};

	bool Load(const std::filesystem::path& FilePath, FSprite& OutputSprite);
}