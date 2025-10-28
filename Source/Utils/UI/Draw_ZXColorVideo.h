#pragma once

#include <CoreMinimal.h>
#include <Core/Image.h>

namespace UI
{
	#define ATTRIBUTE_GRID          1 << 0
	#define GRID					1 << 1
	#define PIXEL_GRID              1 << 2
	#define BEAM_ENABLE				1 << 3
	#define ALPHA_CHECKERBOARD_GRID 1 << 4
	#define PIXEL_CURSOR			1 << 5
	#define FORCE_NEAREST_SAMPLING  1 << 31

	namespace EZXSpectrumColor
	{
		enum Type : uint8_t
		{
			Black = 0,
			Blue,
			Red,
			Magenta,
			Green,
			Cyan,
			Yellow,
			White,

			Black_,
			Blue_,
			Red_,
			Magenta_,
			Green_,
			Cyan_,
			Yellow_,
			White_,

			MAX,

			True,
			False,

			None		= (uint8_t)INDEX_NONE,
			Transparent = Black,
		};
	}

	// 0xABGR
	static constexpr uint32_t ZXSpectrumColorRGBA[EZXSpectrumColor::MAX] =
	{
		(0x00000000),	// Black
		(0x0000BFFF),	// Blue
		(0xBF0000FF),	// Red
		(0xBF00BFFF),	// Magenta
		(0x00BF00FF),	// Green
		(0x00BFBFFF),	// Cyan
		(0xBFBF00FF),	// Yellow
		(0xBFBFBFFF),	// White

		(0x000000FF),	// Black
		(0x0000FFFF),	// Blue
		(0xFF0000FF),	// Red
		(0xFF00FFFF),	// Magenta
		(0x00FF00FF),	// Green
		(0x00FFFFFF),	// Cyan
		(0xFFFF00FF),	// Yellow
		(0xFFFFFFFF),	// White
	};

	namespace ERenderType
	{
		enum Type
		{
			Unknown,
			Screen,
			Canvas,
		};
	}

	struct FConversationSettings
	{
		uint8_t InkAlways = EZXSpectrumColor::Black_;
		uint8_t TransparentIndex = EZXSpectrumColor::Transparent;
		uint8_t ReplaceTransparent = EZXSpectrumColor::White;
	};

	struct FZXViewOptions
	{
		bool bAttributeGrid = false;
		bool bGrid = false;
		bool bPixelGrid = true;
		bool bAlphaCheckerboardGrid = true;
		
		ImVec2 GridSettingSize = ImVec2(8.0f, 8.0f);				// width in UV coords of grid line
		ImVec2 GridSettingOffset = ImVec2(0.0f, 0.0f);
	};

	struct FZXColorView
	{
		// scale
		float ZoomRate = 1.25f;										// how fast mouse wheel affects zoom
		float PixelAspectRatio = 1.0f;								// values other than 1 not supported yet
		float MinimumGridSize = 4.0f;								// don't draw the grid if lines would be closer than MinimumGridSize pixels
		ImVec2 Scale = ImVec2(2.0f, 2.0f);							// 1 pixel is 1 texel
		ImVec2 ScaleMin = ImVec2(1.0f, 1.0f);
		ImVec2 ScaleMax = ImVec2(32.0f, 32.0f);

		// view state
		ImVec2 ImagePosition = ImVec2(0.5f, 0.5f);					// the UV value at the center of the current view
		ImVec2 PanelTopLeftPixel = ImVec2(0.0f, 0.0);				// top left of view in ImGui pixel coordinates
		ImVec2 ViewTopLeftPixel = ImVec2(0.0f, 0.0f);				// position in ImGui pixel coordinates
		ImVec2 ViewSize = ImVec2(0.0f, 0.0f);						// rendered size of current image. this could be smaller than panel size if user has zoomed out.
		ImVec2 ViewSizeUV = ImVec2(0.0f, 0.0f);						// visible region of the texture in UV coordinates
		Transform2D TexelsToPixels;
		Transform2D PixelsToTexels;

		// texture
		ImRect UV = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
		ImVec2 TextureSizePixels = ImVec2(0.0f, 0.0f);

		// device
		ID3D11Device* Device = nullptr;
		ID3D11DeviceContext* DeviceContext = nullptr;

		// shader
		ID3D11Buffer* PSCB_Grid = nullptr;
		ID3D11PixelShader* PS_Grid = nullptr;

		// shader variable
		bool bBeamEnable = false;
		bool bCursorEnable = false;
		bool bForceNearestSampling = true;								// if true fragment shader will always sample from texel centers
		float TimeCounter = 0.0f;
		ImVec2 GridWidth = ImVec2(0.0f, 0.0f);							// width in UV coords of grid line
		ImVec2 CursorPosition = ImVec2(0.0f, 0.0f);
		ImVec4 GridColor = ImVec4(0.025f, 0.025f, 0.15f, 0.0f);
		ImVec4 CursorColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		ImVec4 BackgroundColor = ImVec4(0.0f, 1.0f, 0.0f, 0.0f);		// color used for alpha blending
		ImVec4 TransparentColor = ImVec4(0.169f, 0.396f, 0.925f, 0.0f);	// the color used to display transparency
		FZXViewOptions Options;

		// render data
		FImage Image;						// final display result

		// ZX Spectrum viewing data
		std::vector<uint8_t> IndexedData;	// indexed image data after QuantizeToZX
		std::vector<uint8_t> InkData;		// pixels
		std::vector<uint8_t> AttributeData;	// color ink and color paper (fbpppiii)
											// f - flag, flash (swap color inc and paper)
											// b - flag, bright colors
											// p - 3-bit color paper
											// i - 3-bit color pixel
		std::vector<uint8_t> MaskData;		// auto mask from alpha channel

		// user data
		std::shared_ptr<void> UserData;

		ERenderType::Type RenderType = ERenderType::Unknown;
	};

	ImVec2 GetMouse(std::shared_ptr<UI::FZXColorView> ZXColorView);

	void Draw_ZXColorView_Initialize(std::shared_ptr<UI::FZXColorView>, ERenderType::Type RenderType);
	void Draw_ZXColorView_Shutdown(std::shared_ptr<UI::FZXColorView> ZXColorView);
	void Draw_ZXColorView(std::shared_ptr<UI::FZXColorView> ZXColorView);

	void Set_ZXViewPosition(std::shared_ptr<UI::FZXColorView> ZXColorView, ImVec2 NewPosition);
	void Add_ZXViewDeltaPosition(std::shared_ptr<UI::FZXColorView> ZXColorView, ImVec2 DeltaPosition);
	void Set_ZXViewScale(std::shared_ptr<UI::FZXColorView> ZXColorView, float MouseWheel);
	ImVec2 ConverZXViewPositionToPixel(UI::FZXColorView& ZXColorView, const ImVec2& Position);
	void ConvertZXIndexColorToDisplayRGB(FImage& InOutputImage, const std::vector<uint8_t> Data);


	void GetInkPaper(const std::vector<uint8_t>& IndicesBoundary, uint8_t& OutputPaperColor, uint8_t& OutputInkColor,
		int32_t TransparentIndex = UI::EZXSpectrumColor::Transparent, int32_t ReplaceTransparent = UI::EZXSpectrumColor::White);
	int32_t FindClosestColor(ImU32 Color);
	void QuantizeToZX(uint8_t* RawImage, int32_t Width, int32_t Height, int32_t Channels, std::vector<uint8_t>& OutputIndexedData);
	void ZXIndexColorToRGBA(FImage& InOutputImage, const std::vector<uint8_t>& IndexedData, int32_t Width, int32_t Height, bool bCreate = false);
	void ZXIndexColorToZXAttributeColor(
		const std::vector<uint8_t>& IndexedData, int32_t Width, int32_t Height,
		std::vector<uint8_t>& OutputInkData,
		std::vector<uint8_t>& OutputAttributeData,
		std::vector<uint8_t>& OutputMaskData,
		const UI::FConversationSettings& Settings);
	void ZXAttributeColorToZXIndexColor(
		FImage& InOutputImage, 
		int32_t Width, int32_t Height,
		std::vector<uint8_t>& OutputIndexedData,
		const std::vector<uint8_t>& InkData,
		const std::vector<uint8_t>& AttributeData,
		const std::vector<uint8_t>& MaskData);

	void ZXAttributeColorToImage(
		FImage& InOutputImage,
		int32_t Width, int32_t Height,
		const uint8_t* InkData,
		const uint8_t* AttributeData,
		const uint8_t* MaskData,
		bool bCreate = false,
		std::vector<uint8_t>* OutputIndexedData = nullptr);
}
