#include "Screen.h"

#include "Utils/Shader.h"
#include "resource.h"

namespace
{
	#define ATTRIBUTE_GRID          1 << 0
	#define GRID					1 << 1
	#define PIXEL_GRID              1 << 2
	#define FORCE_NEAREST_SAMPLING  1 << 31

	static const char* ThisWindowName = TEXT("Screen");
	static const char* ResourcePathName = TEXT("SHADER/ZX");

	void DrawCallback(const ImDrawList* ParentList, const ImDrawCmd* CMD)
	{
		const FRenderData* RenderData = CMD ? reinterpret_cast<const FRenderData*>(CMD->UserCallbackData) : nullptr;
		std::shared_ptr<SScreen> Screen = RenderData ? std::dynamic_pointer_cast<SScreen>(RenderData->Screen) : nullptr;
		if (Screen)
		{
			Screen->OnDrawCallback(ParentList, CMD);
		}
	}
}

namespace Shader
{
	/* Constant buffer used in pixel shader. Size must be multiple of 16 bytes.
	 * Layout must match the layout in the pixel shader. */
	struct PIXEL_CONSTANT_BUFFER
	{
		float GridColor[4];
		float GridWidth[2];
		int	  Flags;
		float TimeCounter;
		float BackgroundColor[3];
		int   Dummy_0;
		float TextureSize[2];
		float GridSize[2];
		float GridOffset[2];

		float Dummy[46];
	};

	void* LINE_ID = (void*)0x10FFFFFF;
}

SScreen::SScreen(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))

	// scale
	, ZoomRate(1.25f)										// how fast mouse wheel affects zoom
	, PixelAspectRatio(1.0f)								// values other than 1 not supported yet
	, MinimumGridSize(4.0f)									// don't draw the grid if lines would be closer than MinimumGridSize pixels
	, Scale(1.7f, 1.7f)										// 1 pixel is 1 texel
	, OldScale(1.7f, 1.7f)
	, ScaleMin(1.0f, 1.0f)
	, ScaleMax(32.0f, 32.0f)

	// view state	
	, ImagePosition(0.5f, 0.49f)								// the UV value at the center of the current view
	, PanelTopLeftPixel(0.0f, 0.0)							// top left of view in ImGui pixel coordinates
	, PanelSize(0.0f, 0.0f)									// size of area allocated to drawing the image in pixels.
	, ViewTopLeftPixel(0.0f, 0.0f)							// position in ImGui pixel coordinates
	, ViewSize(0.0f, 0.0f)									// rendered size of current image. this could be smaller than panel size if user has zoomed out.
	, ViewSizeUV(0.0f, 0.0f)								// visible region of the texture in UV coordinates

	// texture
	, UV(0.0f, 0.0f, 0.0f, 0.0f)
	, TextureSizePixels(0.0f, 0.0f)

	// shader
	, PSCB_Grid(nullptr)
	, PS_Grid(nullptr)

	// shader variable
	, TimeCounter(0.0f)
	, bForceNearestSampling(true)							// if true fragment shader will always sample from texel centers
	, GridWidth(0.0f, 0.0f)									// width in UV coords of grid line
	, GridSize(0.0f, 0.0f)
	, GridOffset(0.0f, 0.0f)
	, GridColor(0.025f, 0.025f, 0.15f, 0.0f)
	, BackgroundColor(0.0f, 1.0f, 0.0f, 0.0f)				// color used for alpha blending

	, bDragging(false)										// is user currently dragging to pan view
{
	// at a pixel clock of 7MHz, clock cycles
	ScreenSettings.FlybackH = 96;
	ScreenSettings.BorderL = 48;
	ScreenSettings.DisplayH = 256;
	ScreenSettings.BorderR = 48;
	ScreenSettings.FlybackV = 16;
	ScreenSettings.BorderT = 48;
	ScreenSettings.DisplayV = 192;
	ScreenSettings.BorderB = 56;

	ScreenSettings.Width = ScreenSettings.BorderL + ScreenSettings.DisplayH + ScreenSettings.BorderR;
	ScreenSettings.Height = ScreenSettings.BorderT + ScreenSettings.DisplayV + ScreenSettings.BorderB;
}

void SScreen::Initialize()
{
	DisplayRGBA.resize(ScreenSettings.Width * ScreenSettings.Height );
	for (uint32_t y = 0; y < ScreenSettings.Height; ++y)
	{
		for (uint32_t x = 0; x < ScreenSettings.Width; ++x)
		{
			uint32_t RGBA = 0xFF << 24;
			if (x >= ScreenSettings.BorderL &&
				x < (ScreenSettings.BorderL + ScreenSettings.DisplayH) &&
				y >= ScreenSettings.BorderT &&
				y < (ScreenSettings.BorderT + ScreenSettings.DisplayV))
			{
				uint8_t R = (!!(rand() & 1)) * 255;
				uint8_t G = (!!(rand() & 1)) * 255;
				uint8_t B = (!!(rand() & 1)) * 255;
				RGBA |= (B << 16) | (G << 8) | (R << 0);
			}
			else
			{
				uint8_t R = 255;
				uint8_t G = 255;
				uint8_t B = 255;
				RGBA |= (B << 16) | (G << 8) | (R << 0);
			}
			DisplayRGBA[y * ScreenSettings.Width + x] = RGBA;
		}
	}
	FImageBase& Images = FImageBase::Get();
	Image = Images.CreateTexture(DisplayRGBA.data(), ScreenSettings.Width, ScreenSettings.Height);

	PS_Grid = Shader::CreatePixelShaderFromResource(Data.Device, IDR_PS_GRID);
	PSCB_Grid = Shader::CreatePixelShaderConstantBuffer<Shader::PIXEL_CONSTANT_BUFFER>(Data.Device);
	SpectrumScreen = std::make_shared<FRenderData>(std::dynamic_pointer_cast<SScreen>(shared_from_this()), ERenderType::Screen);
}

void SScreen::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	{
		Input_HotKeys();
		Input_Mouse();
		Draw_Display();

		ImGui::End();
	}
}

void SScreen::Destroy()
{
	Shader::ReleasePixelShader(PS_Grid);
	Shader::ReleaseConstantBuffer(PSCB_Grid);
}

void SScreen::OnDrawCallback(const ImDrawList* ParentList, const ImDrawCmd* CMD)
{
	if (Data.DeviceContext == nullptr || ParentList == nullptr || CMD == nullptr)
	{
		return;
	}
	const FRenderData& RenderData = *reinterpret_cast<const FRenderData*>(CMD->UserCallbackData);

	if (RenderData.RenderType == ERenderType::Screen)
	{
		if (PS_Grid == nullptr || PSCB_Grid == nullptr)
		{
			return;
		}

		// map the pixel shader constant buffer and fill values
		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			if (Data.DeviceContext->Map(PSCB_Grid, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource) != S_OK)
			{
				return;
			}

			// transfer shader options from shaderOptions to our backend specific pixel shader constant buffer
			Shader::PIXEL_CONSTANT_BUFFER* ConstantBuffer = (Shader::PIXEL_CONSTANT_BUFFER*)MappedResource.pData;
			std::memcpy(ConstantBuffer->GridColor, &GridColor, sizeof(GridColor));
			std::memcpy(ConstantBuffer->GridWidth, &GridWidth, sizeof(GridWidth));
			std::memcpy(ConstantBuffer->BackgroundColor, &BackgroundColor, sizeof(float) * 3);
			std::memcpy(ConstantBuffer->TextureSize, &Image.Size, sizeof(Image.Size));

			{
				uint32_t Flags = 0;
				if (ScreenSettings.bAttributeGrid)
				{
					Flags |= ATTRIBUTE_GRID;
				}
				if (ScreenSettings.bGrid)
				{
					Flags |= GRID;
				}
				if (ScreenSettings.bPixelGrid)
				{
					Flags |= PIXEL_GRID;
				}
				if (bForceNearestSampling)
				{
					Flags |= FORCE_NEAREST_SAMPLING;
				}
				ConstantBuffer->Flags = Flags;

				ConstantBuffer->GridSize[0] = ScreenSettings.GridSettingSize.x;
				ConstantBuffer->GridSize[1] = ScreenSettings.GridSettingSize.y;
				ConstantBuffer->GridOffset[0] = ScreenSettings.GridSettingOffset.x;
				ConstantBuffer->GridOffset[1] = ScreenSettings.GridSettingOffset.y;
			}
			Data.DeviceContext->Unmap(PSCB_Grid, 0);
		}

		// activate shader and buffer
		Data.DeviceContext->PSSetShader(PS_Grid, NULL, 0);
		Data.DeviceContext->PSSetConstantBuffers(0, 1, &PSCB_Grid);
	}
}

void SScreen::Draw_Display()
{
	// update shader variable
	{
		if (Scale.y > MinimumGridSize)
		{
			// enable grid in shader
			GridColor.w = 1.0f;
			SetScale(FMath::Round(Scale.y));
		}
		else
		{
			// disable grid in shader
			GridColor.w = 0.0f;
		}
		bForceNearestSampling = (Scale.x > 1.0f || Scale.y > 1.0f);
	}

	ImGuiWindow* Window = ImGui::GetCurrentWindow();

	// keep track of size of area that we draw for borders later
	PanelTopLeftPixel = ImGui::GetCursorScreenPos();
	ImGui::SetCursorPos(ImGui::GetCursorPos() + CalculatePanelSize());
	ViewTopLeftPixel = ImGui::GetCursorScreenPos();
	ImRect Rect(Window->DC.CursorPos, Window->DC.CursorPos + ViewSize);

	// callback for using our own image shader 
	ImGui::GetWindowDrawList()->AddCallback(DrawCallback, SpectrumScreen.get());
	ImGui::GetWindowDrawList()->AddImage(Image.ShaderResourceView, Rect.Min, Rect.Max, UV.Min, UV.Max);

	// reset callback for using our own image shader 
	ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void SScreen::Input_HotKeys()
{
}

void SScreen::Input_Mouse()
{
	bDragging = false;

	ImGuiContext& Context = *GImGui;
	const float MouseWheel = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner) ? Context.IO.MouseWheel : 0.0f;

	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != ImGui::GetCurrentWindow()->ID)
	{
		return;
	}

	if (MouseWheel != 0.0f && Context.IO.KeyCtrl && !Context.IO.FontAllowUserScaling)
	{
		// scale
		{
			TexelsToPixels = GetTexelsToPixels(ViewTopLeftPixel, ViewSize, UV.Min, ViewSizeUV, Image.Size);
			PixelsToTexels = TexelsToPixels.Inverse();

			const ImVec2 MousePosition = ImGui::GetMousePos();
			ImVec2 MousePositionTexel = PixelsToTexels * MousePosition;
			const ImVec2 MouseUV = MousePositionTexel / Image.Size;
			float LocalScale = Scale.y;
			float PrevScale = LocalScale;

			bool keepTexelSizeRegular = LocalScale > MinimumGridSize;
			if (MouseWheel > 0.0f)
			{
				LocalScale *= ZoomRate;
				if (keepTexelSizeRegular)
				{
					// it looks nicer when all the grid cells are the same size
					// so keep scale integer when zoomed in
					LocalScale = ImCeil(LocalScale);
				}
			}
			else
			{
				LocalScale /= ZoomRate;
				if (keepTexelSizeRegular)
				{
					// see comment above. We're doing a floor this time to make
					// sure the scale always changes when scrolling
					LocalScale = FMath::ImFloorSigned(LocalScale);
				}
			}
			/* to make it easy to get back to 1:1 size we ensure that we stop
			 * here without going straight past it*/
			if ((PrevScale < 1.0f && LocalScale > 1.0f) || (PrevScale > 1.0f && LocalScale < 1.0f))
			{
				LocalScale = 1.0f;
			}
			SetScale(ImVec2(PixelAspectRatio * LocalScale, LocalScale));
			if (LocalScale <= ScaleMax.y && LocalScale >= ScaleMin.y)
			{
				SetImagePosition(ImagePosition + (MouseUV - ImagePosition) * (1.0f - PrevScale / LocalScale));
			}
		}
	}
	else if (!bDragging && Context.IO.MouseDown[ImGuiMouseButton_Middle])
	{
		bDragging = true;
	}
	else if (Context.IO.MouseReleased[ImGuiMouseButton_Middle])
	{
		bDragging = false;
	}
	// dragging
	if (bDragging)
	{
		ImVec2 uvDelta = ImGui::GetIO().MouseDelta * ViewSizeUV / ViewSize;
		ImagePosition -= uvDelta;
		RoundImagePosition();
	}
}

void SScreen::RoundImagePosition()
{
	/* when ShowWrap mode is disabled the limits are a bit more strict. We
	 * try to keep it so that the user cannot pan past the edge of the
	 * texture at all.*/
	ImVec2 AbsViewSizeUV = FMath::Abs(ViewSizeUV);
	ImagePosition = ImMax(ImagePosition - AbsViewSizeUV * 0.5f, ImVec2(0.0f, 0.0f)) + AbsViewSizeUV * 0.5f;
	ImagePosition = ImMin(ImagePosition + AbsViewSizeUV * 0.5f, ImVec2(1.0f, 1.0f)) - AbsViewSizeUV * 0.5f;

	/* if inspector->scale is 1 then we should ensure that pixels are aligned
	 * with texel centers to get pixel-perfect texture rendering*/
	ImVec2 TopLeftSubTexel = ImagePosition * Scale * Image.Size - ViewSize * 0.5f;

	if (Scale.x >= 1.0f)
	{
		TopLeftSubTexel.x = FMath::Round(TopLeftSubTexel.x);
	}
	if (Scale.y >= 1.0f)
	{
		TopLeftSubTexel.y = FMath::Round(TopLeftSubTexel.y);
	}
	ImagePosition = (TopLeftSubTexel + ViewSize * 0.5f) / (Scale * Image.Size);
}

ImVec2 SScreen::CalculatePanelSize()
{
	// calculate panel size
	const float BorderWidth = 1.0f;
	ImVec2 ContentRegionAvail = ImGui::GetContentRegionAvail();
	ImVec2 AvailablePanelSize = ContentRegionAvail - ImVec2(BorderWidth, BorderWidth) * 2.0f;

	RoundImagePosition();

	TextureSizePixels = Image.Size * Scale;
	ViewSizeUV = AvailablePanelSize / TextureSizePixels;
	UV.Min = ImagePosition - ViewSizeUV * 0.5f;
	UV.Max = ImagePosition + ViewSizeUV * 0.5f;

	ImVec2 DrawImageOffset(BorderWidth, BorderWidth);
	ViewSize = AvailablePanelSize;

	// don't crop the texture to UV [0,1] range.  What you see outside this range will depend on API and texture properties
	if (TextureSizePixels.x < AvailablePanelSize.x)
	{
		// not big enough to horizontally fill view
		ViewSize.x = ImFloor(TextureSizePixels.x);
		DrawImageOffset.x += ImFloor((AvailablePanelSize.x - TextureSizePixels.x) * 0.5f);
		UV.Min.x = 0.0f;
		UV.Max.x = 1.0f;
		ViewSizeUV.x = 1.0f;
		ImagePosition.x = 0.5f;
	}
	if (TextureSizePixels.y < AvailablePanelSize.y)
	{
		// not big enough to vertically fill view
		ViewSize.y = ImFloor(TextureSizePixels.y);
		DrawImageOffset.y += ImFloor((AvailablePanelSize.y - TextureSizePixels.y) * 0.5f);
		UV.Min.y = 0.0f;
		UV.Max.y = 1.0f;
		ViewSizeUV.y = 1.0f;
		ImagePosition.y = 0.5f;
	}

	return DrawImageOffset;
}

void SScreen::SetScale(float scaleY)
{
	SetScale(ImVec2(scaleY * PixelAspectRatio, scaleY));
}

void SScreen::SetScale(ImVec2 NewScale)
{
	NewScale = ImClamp(NewScale, ScaleMin, ScaleMax);
	ViewSizeUV *= Scale / NewScale;
	Scale = NewScale;

	// only force nearest sampling if zoomed in
	bForceNearestSampling = (Scale.x > 1.0f || Scale.y > 1.0f);
	GridWidth = ImVec2(1.0f / Scale.x, 1.0f / Scale.y);
}

void SScreen::SetImagePosition(ImVec2 NewPosition)
{
	ImagePosition = NewPosition;
	RoundImagePosition();
}

ImVec2 SScreen::ConverPositionToPixel(const ImVec2& Position)
{
	const ImVec2 ImageSizeInv = ImVec2(1.0f, 1.0f) / Image.Size;
	return ImFloor((Position - ViewTopLeftPixel + UV.Min / ImageSizeInv * Scale) / Scale);
}

Transform2D SScreen::GetTexelsToPixels(const ImVec2& ScreenTopLeft, const ImVec2& ScreenViewSize, const ImVec2& UVTopLeft, const ImVec2& UVViewSize, const ImVec2& TextureSize)
{
	const ImVec2 UVToPixel = ScreenViewSize / UVViewSize;
	Transform2D Transform;
	Transform.Scale = UVToPixel / TextureSize;
	Transform.Translate.x = ScreenTopLeft.x - UVTopLeft.x * UVToPixel.x;
	Transform.Translate.y = ScreenTopLeft.y - UVTopLeft.y * UVToPixel.y;
	return Transform;
}
