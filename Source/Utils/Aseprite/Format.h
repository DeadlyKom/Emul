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
		std::filesystem::path Path;
		std::filesystem::path Name;
	};


	namespace ELayerFlags
	{
		enum Type
		{
			None = 0,
			Visible = 1,     // Can be read
			Editable = 2,    // Can be written
			LockMove = 4,    // Cannot be moved
			Background = 8,  // Stack order cannot be changed
			Continuous = 16, // Prefer to link cels when the user copy them
			Collapsed = 32,  // Prefer to show this group layer collapsed
			Reference = 64,  // Is a reference layer

			PersistentFlagsMask = 0xffff,
			Internal_WasVisible = 0x10000, // Was visible in the alternative state (Alt+click)

			BackgroundLayerFlags = LockMove | Background,

			// Flags that change the modified flag of the document
			// (e.g. created by undoable actions).
			StructuralFlagsMask = Background | Reference,
		};
	}

	struct FLayer
	{
		uint16_t Flags;		// ELayerFlags
		uint16_t LayerType;
		uint16_t ChildLevel;
		uint16_t DefaultWidth;
		uint16_t DefaultHeight;
		uint16_t Blendmode;
		uint8_t Opacity;
		std::string Name;
	};

	struct FSprite
	{
		EColorMode ColorMode;
		int32_t Width;
		int32_t Height;
		uint32_t TransparentColor;

		std::vector<std::vector<uint8_t>> Frames;
		std::vector<int32_t> DurationPerFrame;
		std::vector<FLayer> Layers;
	};

	bool Load(const std::filesystem::path& FilePath, FSprite& OutputSprite);
}