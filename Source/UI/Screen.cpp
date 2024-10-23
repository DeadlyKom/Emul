#include "Screen.h"

#include "AppDebugger.h"
#include "Utils/UI.h"
#include "Utils/Shader.h"
#include "Motherboard/Motherboard.h"
#include "Devices/ControlUnit/Interface_Display.h"
#include "resource.h"

namespace
{
	#define ATTRIBUTE_GRID          1 << 0
	#define GRID					1 << 1
	#define PIXEL_GRID              1 << 2
	#define BEAM_ENABLE				1 << 3
	#define FORCE_NEAREST_SAMPLING  1 << 31

	namespace ZXSpectrumColor
	{
		enum Type
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
		};
	}

	// 0xABGR
	ImU32 ZXSpectrumColorRGBA[ZXSpectrumColor::MAX] = {

		(0x00000000),	// Black
		(0x007F0000),	// Blue
		(0x0000007F),	// Red
		(0x007F007F),	// Magenta
		(0x00007F00),	// Green
		(0x007F7F00),	// Cyan
		(0x00007F7F),	// Yellow
		(0x007F7F7F),	// White

		(0x00000000),	// Black
		(0x00FF0000),	// Blue
		(0x000000FF),	// Red
		(0x00FF00FF),	// Magenta
		(0x0000FF00),	// Green
		(0x00FFFF00),	// Cyan
		(0x0000FFFF),	// Yellow
		(0x00FFFFFF),	// White
	};

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
		float CRT_BeamPosition[2];

		float Dummy[44];
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
	, Scale(1.9f, 1.9f)										// 1 pixel is 1 texel
	, OldScale(1.9f, 1.9f)
	, ScaleMin(1.0f, 1.0f)
	, ScaleMax(32.0f, 32.0f)

	// view state	
	, ImagePosition(0.5f, 0.48f)							// the UV value at the center of the current view
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
	, bBeamEnable(false)
	, bForceNearestSampling(true)							// if true fragment shader will always sample from texel centers
	, GridWidth(0.0f, 0.0f)									// width in UV coords of grid line
	, GridSize(0.0f, 0.0f)
	, GridOffset(0.0f, 0.0f)
	, GridColor(0.025f, 0.025f, 0.15f, 0.0f)
	, BackgroundColor(0.0f, 1.0f, 0.0f, 0.0f)				// color used for alpha blending

	, bDragging(false)										// is user currently dragging to pan view
{}

void SScreen::Initialize()
{
	const FSpectrumDisplay SD = GetMotherboard().GetState<FSpectrumDisplay>(NAME_MainBoard, NAME_ULA);

	ScreenSettings.DisplayCycles = SD.DisplayCycles;
	ScreenSettings.Width = ScreenSettings.DisplayCycles.BorderL + ScreenSettings.DisplayCycles.DisplayH + ScreenSettings.DisplayCycles.BorderR;
	ScreenSettings.Height = ScreenSettings.DisplayCycles.BorderT + ScreenSettings.DisplayCycles.DisplayV + ScreenSettings.DisplayCycles.BorderB;
	DisplayRGBA.resize(ScreenSettings.Width * ScreenSettings.Height);

	FImageBase& Images = FImageBase::Get();
	Image = Images.CreateTexture(DisplayRGBA.data(), ScreenSettings.Width, ScreenSettings.Height, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC);
	ConvertDisplayDataToRGB(SD);

	PS_Grid = Shader::CreatePixelShaderFromResource(Data.Device, IDR_PS_GRID);
	PSCB_Grid = Shader::CreatePixelShaderConstantBuffer<Shader::PIXEL_CONSTANT_BUFFER>(Data.Device);
	SpectrumScreen = std::make_shared<FRenderData>(std::dynamic_pointer_cast<SScreen>(shared_from_this()), ERenderType::Screen);
}

void SScreen::Tick(float DeltaTime)
{
	Status = GetMotherboard().GetState<EThreadStatus>(NAME_MainBoard, NAME_None);
	if (Status == EThreadStatus::Run)
	{
		ConvertDisplayDataToRGB(GetMotherboard().GetState<FSpectrumDisplay>(NAME_MainBoard, NAME_ULA));
	}
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
				ImVec2 CRT_BeamPosition = SpectrumDisplay.CRT_BeamPosition / Image.Size;
				std::memcpy(ConstantBuffer->CRT_BeamPosition, &CRT_BeamPosition, sizeof(CRT_BeamPosition));
			}

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
				if (bBeamEnable)
				{
					Flags |= BEAM_ENABLE;
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

FMotherboard& SScreen::GetMotherboard() const
{
	return *FAppFramework::Get<FAppDebugger>().Motherboard;
}

void SScreen::Load_MemoryScreen()
{
	SpectrumDisplay = GetMotherboard().GetState<FSpectrumDisplay>(NAME_MainBoard, NAME_ULA);
	ConvertDisplayDataToRGB(SpectrumDisplay);
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

void SScreen::OnInputStep(FCPU_StepType Type)
{
	Load_MemoryScreen();
}

void SScreen::OnInputDebugger(bool bDebuggerState)
{
	bBeamEnable = bDebuggerState /*true = enter debugger*/;
	if (bDebuggerState)
	{
		Load_MemoryScreen();
	}
}

void SScreen::ConvertDisplayDataToRGB(const FSpectrumDisplay& DS)
{
	for (int32_t i = 0; i < DS.DisplayData.size(); ++i)
	{
		const uint8_t& Value = DS.DisplayData[i];
		const ZXSpectrumColor::Type IndexColor = static_cast<ZXSpectrumColor::Type>(Value);
		const uint32_t RGBA = (0xFF << 24) | ZXSpectrumColorRGBA[IndexColor];
		DisplayRGBA[i] = RGBA;
	}

	FImageBase& Images = FImageBase::Get();
	Images.UpdateTexture(Image.Handle, DisplayRGBA.data());
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
