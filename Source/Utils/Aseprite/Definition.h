#pragma once

namespace AsepriteFormat
{
	#define ASE_FILE_MAGIC                   0xA5E0
	#define ASE_FILE_FRAME_MAGIC             0xF1FA

	#define ASE_FILE_FLAG_LAYER_WITH_OPACITY 1
	#define ASE_FILE_FLAG_COMPOSITE_GROUPS   2
	#define ASE_FILE_FLAG_LAYER_WITH_UUID    4

	#define ASE_FILE_CHUNK_FLI_COLOR2        4
	#define ASE_FILE_CHUNK_FLI_COLOR         11
	#define ASE_FILE_CHUNK_LAYER             0x2004
	#define ASE_FILE_CHUNK_CEL               0x2005
	#define ASE_FILE_CHUNK_CEL_EXTRA         0x2006
	#define ASE_FILE_CHUNK_COLOR_PROFILE     0x2007
	#define ASE_FILE_CHUNK_EXTERNAL_FILE     0x2008
	#define ASE_FILE_CHUNK_MASK              0x2016
	#define ASE_FILE_CHUNK_PATH              0x2017
	#define ASE_FILE_CHUNK_TAGS              0x2018
	#define ASE_FILE_CHUNK_PALETTE           0x2019
	#define ASE_FILE_CHUNK_USER_DATA         0x2020
	#define ASE_FILE_CHUNK_SLICES                                                                      \
	  0x2021 // Deprecated chunk (used on dev versions only between v1.2-beta7 and v1.2-beta8)
	#define ASE_FILE_CHUNK_SLICE              0x2022
	#define ASE_FILE_CHUNK_TILESET            0x2023

	#define ASE_FILE_LAYER_IMAGE              0
	#define ASE_FILE_LAYER_GROUP              1
	#define ASE_FILE_LAYER_TILEMAP            2

	#define ASE_FILE_RAW_CEL                  0
	#define ASE_FILE_LINK_CEL                 1
	#define ASE_FILE_COMPRESSED_CEL           2
	#define ASE_FILE_COMPRESSED_TILEMAP       3

	#define ASE_FILE_NO_COLOR_PROFILE         0
	#define ASE_FILE_SRGB_COLOR_PROFILE       1
	#define ASE_FILE_ICC_COLOR_PROFILE        2

	#define ASE_COLOR_PROFILE_FLAG_GAMMA      1

	#define ASE_PALETTE_FLAG_HAS_NAME         1

	#define ASE_USER_DATA_FLAG_HAS_TEXT       1
	#define ASE_USER_DATA_FLAG_HAS_COLOR      2
	#define ASE_USER_DATA_FLAG_HAS_PROPERTIES 4

	#define ASE_CEL_EXTRA_FLAG_PRECISE_BOUNDS 1

	#define ASE_SLICE_FLAG_HAS_CENTER_BOUNDS  1
	#define ASE_SLICE_FLAG_HAS_PIVOT_POINT    2

	#define ASE_TILESET_FLAG_EXTERNAL_FILE    1
	#define ASE_TILESET_FLAG_EMBEDDED         2
	#define ASE_TILESET_FLAG_ZERO_IS_NOTILE   4
	#define ASE_TILESET_FLAG_MATCH_XFLIP      8
	#define ASE_TILESET_FLAG_MATCH_YFLIP      16
	#define ASE_TILESET_FLAG_MATCH_DFLIP      32

	#define ASE_EXTERNAL_FILE_PALETTE         0
	#define ASE_EXTERNAL_FILE_TILESET         1
	#define ASE_EXTERNAL_FILE_EXTENSION       2
	#define ASE_EXTERNAL_FILE_TILE_MANAGEMENT 3
	#define ASE_EXTERNAL_FILE_TYPES           4

	struct FAsepriteHeader
	{
		long Pos; // TODO used by the encoder in app project

		uint32_t Size;
		uint16_t Magic;
		uint16_t Frames;
		uint16_t Width;
		uint16_t Height;
		uint16_t Depth;
		uint32_t Flags;
		uint16_t Speed; // Deprecated, use "duration" of AsepriteFrameHeader
		uint32_t Next;
		uint32_t Frit;
		uint8_t TransparentIndex;
		uint8_t Ignore[3];
		uint16_t nColors;
		uint8_t PixelWidth;
		uint8_t PixelHeight;
		int16_t GridX;
		int16_t GridY;
		uint16_t GridWidth;
		uint16_t GridHeight;
	};

	struct FAsepriteFrameHeader
	{
		uint32_t Size;
		uint16_t Magic;
		uint32_t Chunks;
		uint16_t Duration;
	};

	struct FAsepriteChunk
	{
		int Type;
		int Start;
	};
}