#include "Draw_ZXColorVideo.h"
#include "Utils/Shader.h"
#include <Utils/UI/Draw.h>
#include "Devices/ControlUnit/Interface_Display.h"
#include "resource.h"
#include <Window/Sprite/SpriteList.h>

namespace Shader
{
	/* Constant buffer used in pixel shader. Size must be multiple of 16 bytes.
	 * Layout must match the layout in the pixel shader. */
	struct PIXEL_CONSTANT_BUFFER
	{
		float GridColor[4];
		float CursorColor[4];
		float TransparentColor[4];
		float GridWidth[2];
		int	  Flags;
		float TimeCounter;
		float BackgroundColor[3];
		int   Dummy_0;
		float TextureSize[2];
		float GridSize[2];
		float GridOffset[2];
		float CRT_BeamPosition[2];
		float CursorPosition[2];

		float Dummy[38];
	};
}

void SetScale(UI::FZXColorView& ZXColorView, ImVec2 NewScale)
{
	NewScale = ImClamp(NewScale, ZXColorView.ScaleMin, ZXColorView.ScaleMax);
	ZXColorView.ViewSizeUV *= ZXColorView.Scale / NewScale;
	ZXColorView.Scale = NewScale;

	// only force nearest sampling if zoomed in
	ZXColorView.bForceNearestSampling = (ZXColorView.Scale.x > 1.0f || ZXColorView.Scale.y > 1.0f);
	ZXColorView.GridWidth = ImVec2(1.0f / ZXColorView.Scale.x, 1.0f / ZXColorView.Scale.y);
}

void SetScale(UI::FZXColorView& ZXColorView, float ScaleY)
{
	SetScale(ZXColorView, ImVec2(ScaleY * ZXColorView.PixelAspectRatio, ScaleY));
}

void RoundImagePosition(UI::FZXColorView& ZXColorView)
{
	/* when ShowWrap mode is disabled the limits are a bit more strict. We
	 * try to keep it so that the user cannot pan past the edge of the
	 * texture at all.*/
	ImVec2 AbsViewSizeUV = FMath::Abs(ZXColorView.ViewSizeUV);
	ZXColorView.ImagePosition = ImMax(ZXColorView.ImagePosition - AbsViewSizeUV * 0.5f, ImVec2(0.0f, 0.0f)) + AbsViewSizeUV * 0.5f;
	ZXColorView.ImagePosition = ImMin(ZXColorView.ImagePosition + AbsViewSizeUV * 0.5f, ImVec2(1.0f, 1.0f)) - AbsViewSizeUV * 0.5f;

	/* if inspector->scale is 1 then we should ensure that pixels are aligned
	 * with texel centers to get pixel-perfect texture rendering*/
	ImVec2 TopLeftSubTexel = ZXColorView.ImagePosition * ZXColorView.Scale * ZXColorView.Image.Size - ZXColorView.ViewSize * 0.5f;

	if (ZXColorView.Scale.x >= 1.0f)
	{
		TopLeftSubTexel.x = /*FMath::Round*/(TopLeftSubTexel.x);
	}
	if (ZXColorView.Scale.y >= 1.0f)
	{
		TopLeftSubTexel.y = /*FMath::Round*/(TopLeftSubTexel.y);
	}
	ZXColorView.ImagePosition = (TopLeftSubTexel + ZXColorView.ViewSize * 0.5f) / (ZXColorView.Scale * ZXColorView.Image.Size);
}

ImVec2 CalculatePanelSize(UI::FZXColorView& ZXColorView)
{
	// calculate panel size
	const float BorderWidth = 1.0f;
	ImVec2 ContentRegionAvail = ImGui::GetContentRegionAvail();
	ImVec2 AvailablePanelSize = ContentRegionAvail - ImVec2(BorderWidth, BorderWidth) * 2.0f;

	RoundImagePosition(ZXColorView);

	ZXColorView.TextureSizePixels = ZXColorView.Image.Size * ZXColorView.Scale;
	ZXColorView.ViewSizeUV = AvailablePanelSize / ZXColorView.TextureSizePixels;
	ZXColorView.UV.Min = ZXColorView.ImagePosition - ZXColorView.ViewSizeUV * 0.5f;
	ZXColorView.UV.Max = ZXColorView.ImagePosition + ZXColorView.ViewSizeUV * 0.5f;

	ImVec2 DrawImageOffset(BorderWidth, BorderWidth);
	ZXColorView.ViewSize = AvailablePanelSize;

	// don't crop the texture to UV [0,1] range.  What you see outside this range will depend on API and texture properties
	if (ZXColorView.TextureSizePixels.x < AvailablePanelSize.x)
	{
		// not big enough to horizontally fill view
		ZXColorView.ViewSize.x = ImFloor(ZXColorView.TextureSizePixels.x);
		DrawImageOffset.x += ImFloor((AvailablePanelSize.x - ZXColorView.TextureSizePixels.x) * 0.5f);
		ZXColorView.UV.Min.x = 0.0f;
		ZXColorView.UV.Max.x = 1.0f;
		ZXColorView.ViewSizeUV.x = 1.0f;
		ZXColorView.ImagePosition.x = 0.5f;
	}
	if (ZXColorView.TextureSizePixels.y < AvailablePanelSize.y)
	{
		// not big enough to vertically fill view
		ZXColorView.ViewSize.y = ImFloor(ZXColorView.TextureSizePixels.y);
		DrawImageOffset.y += ImFloor((AvailablePanelSize.y - ZXColorView.TextureSizePixels.y) * 0.5f);
		ZXColorView.UV.Min.y = 0.0f;
		ZXColorView.UV.Max.y = 1.0f;
		ZXColorView.ViewSizeUV.y = 1.0f;
		ZXColorView.ImagePosition.y = 0.5f;
	}

	return DrawImageOffset;
}

void UI::OnDrawCallback_ZXVideo(const ImDrawList* ParentList, const ImDrawCmd* CMD)
{
	const UI::FZXColorView* ZXColorView = CMD ? reinterpret_cast<const UI::FZXColorView*>(CMD->UserCallbackData) : nullptr;
	if (!ZXColorView || ZXColorView->PS_Grid == nullptr || ZXColorView->PCB_Grid == nullptr || ZXColorView->Image.SamplerState == nullptr)
	{
		return;
	}

	// map the pixel shader constant buffer and fill values
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		if (ZXColorView->DeviceContext->Map(ZXColorView->PCB_Grid, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource) != S_OK)
		{
			return;
		}

		// transfer shader options from shaderOptions to our backend specific pixel shader constant buffer
		Shader::PIXEL_CONSTANT_BUFFER* ConstantBuffer = (Shader::PIXEL_CONSTANT_BUFFER*)MappedResource.pData;
		std::memcpy(ConstantBuffer->GridColor, &ZXColorView->GridColor, sizeof(ZXColorView->GridColor));
		std::memcpy(ConstantBuffer->CursorColor, &ZXColorView->CursorColor, sizeof(ZXColorView->CursorColor));
		std::memcpy(ConstantBuffer->TransparentColor, &ZXColorView->TransparentColor, sizeof(ZXColorView->TransparentColor));
		std::memcpy(ConstantBuffer->GridWidth, &ZXColorView->GridWidth, sizeof(ZXColorView->GridWidth));
		std::memcpy(ConstantBuffer->BackgroundColor, &ZXColorView->BackgroundColor, sizeof(float) * 3);
		std::memcpy(ConstantBuffer->TextureSize, &ZXColorView->Image.Size, sizeof(ZXColorView->Image.Size));
			
		{
			const ImVec2 CursorPosition = (ZXColorView->CursorPosition / ZXColorView->Image.Size);
			std::memcpy(ConstantBuffer->CursorPosition, &CursorPosition, sizeof(CursorPosition));
		}

		const FSpectrumDisplay* SpectrumDisplay = reinterpret_cast<const FSpectrumDisplay*>(ZXColorView->UserData.get());
		if (SpectrumDisplay != nullptr)
		{
			const ImVec2 CRT_BeamPosition = SpectrumDisplay->CRT_BeamPosition / ZXColorView->Image.Size;
			std::memcpy(ConstantBuffer->CRT_BeamPosition, &CRT_BeamPosition, sizeof(CRT_BeamPosition));
		}

		{
			uint32_t Flags = 0;
			if (ZXColorView->Options.bAttributeGrid)
			{
				Flags |= ATTRIBUTE_GRID;
			}
			if (ZXColorView->Options.bGrid)
			{
				Flags |= GRID;
			}
			if (ZXColorView->Options.bPixelGrid)
			{
				Flags |= PIXEL_GRID;
			}
			if (ZXColorView->Options.bAlphaTransparent)
			{
				Flags |= ALPHA_TRANSPARENT;
			}
			if (ZXColorView->Options.bAlphaCheckerboardGrid)
			{
				Flags |= ALPHA_CHECKERBOARD_GRID;
			}
			if (ZXColorView->bBeamEnable)
			{
				Flags |= BEAM_ENABLE;
			}
			if (ZXColorView->bCursorEnable)
			{
				Flags |= PIXEL_CURSOR;
			}
			if (ZXColorView->bOnlyNearestSampling)
			{
				Flags |= ONLY_NEAREST_SAMPLING;
			}
			if (ZXColorView->bForceNearestSampling)
			{
				Flags |= FORCE_NEAREST_SAMPLING;
			}
			ConstantBuffer->Flags = Flags;

			ConstantBuffer->GridSize[0] = ZXColorView->Options.GridSettingSize.x;
			ConstantBuffer->GridSize[1] = ZXColorView->Options.GridSettingSize.y;
			ConstantBuffer->GridOffset[0] = ZXColorView->Options.GridSettingOffset.x;
			ConstantBuffer->GridOffset[1] = ZXColorView->Options.GridSettingOffset.y;
		}
		ZXColorView->DeviceContext->Unmap(ZXColorView->PCB_Grid, 0);
	}

	// activate shader and buffer
	ZXColorView->DeviceContext->PSSetShader(ZXColorView->PS_Grid, NULL, 0);
	ZXColorView->DeviceContext->PSSetConstantBuffers(0, 1, &ZXColorView->PCB_Grid);
	ZXColorView->DeviceContext->PSSetSamplers(0, 1, &ZXColorView->Image.SamplerState);
}

void OnDrawCallback_LineMarchingAnts(const ImDrawList* ParentList, const ImDrawCmd* CMD)
{
	const UI::FZXColorView* ZXColorView = CMD ? reinterpret_cast<const UI::FZXColorView*>(CMD->UserCallbackData) : nullptr;
	if (!ZXColorView || ZXColorView->PS_LineMarchingAnts == nullptr || ZXColorView->PCB_MarchingAnts == nullptr)
	{
		return;
	}

	// map the pixel shader constant buffer and fill values
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		if (ZXColorView->DeviceContext->Map(ZXColorView->PCB_MarchingAnts, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource) != S_OK)
		{
			return;
		}
		// transfer shader options from shaderOptions to our backend specific pixel shader constant buffer
		Shader::PIXEL_CONSTANT_BUFFER* ConstantBuffer = (Shader::PIXEL_CONSTANT_BUFFER*)MappedResource.pData;
		ConstantBuffer->TimeCounter = ZXColorView->TimeCounter;
		ZXColorView->DeviceContext->Unmap(ZXColorView->PCB_MarchingAnts, 0);
	}
	// Activate shader and buffer
	ZXColorView->DeviceContext->PSSetShader(ZXColorView->PS_LineMarchingAnts, NULL, 0);
	ZXColorView->DeviceContext->PSSetConstantBuffers(0, 1, &ZXColorView->PCB_MarchingAnts);
}

void UI::Draw_ZXColorView_Initialize(std::shared_ptr<UI::FZXColorView> ZXColorView, ERenderType::Type RenderType)
{
	ZXColorView->PS_Grid = Shader::CreatePixelShaderFromResource(ZXColorView->Device, IDR_PS_GRID);
	ZXColorView->PCB_Grid = Shader::CreatePixelShaderConstantBuffer<Shader::PIXEL_CONSTANT_BUFFER>(ZXColorView->Device);
	if (RenderType == ERenderType::Canvas)
	{
		ZXColorView->PS_LineMarchingAnts = Shader::CreatePixelShaderFromResource(ZXColorView->Device, IDR_PS_MA_LINE);
		ZXColorView->PCB_MarchingAnts = Shader::CreatePixelShaderConstantBuffer<Shader::PIXEL_CONSTANT_BUFFER>(ZXColorView->Device);
	}
}

void UI::Draw_ZXColorView_Shutdown(std::shared_ptr<UI::FZXColorView> ZXColorView)
{
	Shader::ReleasePixelShader(ZXColorView->PS_Grid);
	Shader::ReleaseConstantBuffer(ZXColorView->PCB_Grid);
	Shader::ReleasePixelShader(ZXColorView->PS_LineMarchingAnts);
	Shader::ReleaseConstantBuffer(ZXColorView->PCB_MarchingAnts);
	ZXColorView->UserData.reset();
}

bool ClampLineToRect(ImVec2& p1, ImVec2& p2, const ImRect& rect)
{
	// horizontal line
	if (p1.y == p2.y)
	{
		if (p1.y <= rect.Min.y || p1.y >= rect.Max.y)
		{
			// completely outside the Y-axis
			return false;
		}
		p1.x = ImClamp(p1.x, rect.Min.x, rect.Max.x);
		p2.x = ImClamp(p2.x, rect.Min.x, rect.Max.x);
		return (p1.x < p2.x);
	}
	// vertical line
	if (p1.x == p2.x)
	{
		if (p1.x <= rect.Min.x || p1.x >= rect.Max.x)
		{
			// completely outside the X-axis
			return false;
		}
		p1.y = ImClamp(p1.y, rect.Min.y, rect.Max.y);
		p2.y = ImClamp(p2.y, rect.Min.y, rect.Max.y);
		return (p1.y < p2.y);
	}
	return false;
}

void Draw_RectangleMarquee(std::shared_ptr<UI::FZXColorView> ZXColorView, const ImRect& VisibleRect, ImRect* RectangleRect = nullptr)
{	
	ImRect RectangleMarqueeRectTmp = RectangleRect != nullptr ? *RectangleRect : ZXColorView->RectangleMarqueeRect;

	// convert to local pixel coordinates
	const ImVec2 Floor = ImFloor(ZXColorView->UV.Min * ZXColorView->Image.Size);
	RectangleMarqueeRectTmp.Min = (RectangleMarqueeRectTmp.Min - Floor) * ZXColorView->Scale;
	RectangleMarqueeRectTmp.Max = (RectangleMarqueeRectTmp.Max - Floor) * ZXColorView->Scale;

	// clamp to the image boundaries
	//const ImVec2 ImgScaled = ZXColorView->Image.Size * ZXColorView->Scale;
	//RectangleMarqueeRectTmp.Min = ImClamp(RectangleMarqueeRectTmp.Min, ImVec2(0, 0), ImgScaled);
	//RectangleMarqueeRectTmp.Max = ImClamp(RectangleMarqueeRectTmp.Max, ImVec2(0, 0), ImgScaled);

	// screen position
	const ImVec2 TopLeftSubTexel = (ZXColorView->ImagePosition * ZXColorView->Scale * ZXColorView->Image.Size) - ZXColorView->ViewSize * 0.5f;
	const ImVec2 TopLeftPixel = (ZXColorView->ViewTopLeftPixel - (TopLeftSubTexel - ImFloor(TopLeftSubTexel / ZXColorView->Scale) * ZXColorView->Scale));

	ImVec2 A = TopLeftPixel + RectangleMarqueeRectTmp.Min;
	ImVec2 B = TopLeftPixel + RectangleMarqueeRectTmp.Max;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->_FringeScale = 0.0625f;
	DrawList->AddCallback(OnDrawCallback_LineMarchingAnts, ZXColorView.get());

	const ImU32 Color = ImGui::GetColorU32(ImGuiCol_Button);
	const float Thickness = 1.0f;// ImMax(ZXColorView->TexelsToPixels.Scale.x, 1.0f) / ZXColorView->Scale.x;

	// crop to visible area
	ImVec2 VisibleMin = VisibleRect.Min;
	ImVec2 VisibleMax = VisibleRect.Max;

	// we check whether the rectangle is at least partially visible
	if (B.x < VisibleMin.x || A.x > VisibleMax.x || B.y < VisibleMin.y || A.y > VisibleMax.y)
	{
		// completely behind the screen - no drawing
		return;
	}

	// clamping the lines
	ImVec2 Top1 = ImClamp(ImVec2(A.x, A.y), VisibleMin, VisibleMax);
	ImVec2 Top2 = ImClamp(ImVec2(B.x, A.y), VisibleMin, VisibleMax);
	ImVec2 Bottom1 = ImClamp(ImVec2(A.x, B.y), VisibleMin, VisibleMax);
	ImVec2 bottom2 = ImClamp(ImVec2(B.x, B.y), VisibleMin, VisibleMax);
	ImVec2 Left1 = ImClamp(ImVec2(A.x, A.y), VisibleMin, VisibleMax);
	ImVec2 Left2 = ImClamp(ImVec2(A.x, B.y), VisibleMin, VisibleMax);
	ImVec2 Right1 = ImClamp(ImVec2(B.x, A.y), VisibleMin, VisibleMax);
	ImVec2 Right2 = ImClamp(ImVec2(B.x, B.y), VisibleMin, VisibleMax);

	// draw only visible lines
	if (ClampLineToRect(Top1, Top2, VisibleRect))
	{
		DrawList->AddLine(Top1, Top2, Color, Thickness);
	}
	if (ClampLineToRect(Bottom1, bottom2, VisibleRect))
	{
		DrawList->AddLine(Bottom1, bottom2, Color, Thickness);
	}
	if (ClampLineToRect(Left1, Left2, VisibleRect))
	{
		DrawList->AddLine(Left1, Left2, Color, Thickness);
	}
	if (ClampLineToRect(Right1, Right2, VisibleRect))
	{
		DrawList->AddLine(Right1, Right2, Color, Thickness);
	}
}

void UI::Draw_ZXColorView(std::shared_ptr<UI::FZXColorView> ZXColorView)
{
	// update shader variable
	{
		if (ZXColorView->Scale.y > ZXColorView->MinimumGridSize)
		{
			// enable grid in shader
			ZXColorView->GridColor.w = 1.0f;
			SetScale(*ZXColorView, FMath::Round(ZXColorView->Scale.y));
		}
		else
		{
			// disable grid in shader
			ZXColorView->GridColor.w = 0.0f;
		}
		ZXColorView->bForceNearestSampling = (ZXColorView->Scale.x > 1.0f || ZXColorView->Scale.y > 1.0f);
	}

	ImGuiWindow* Window = ImGui::GetCurrentWindow();

	// keep track of size of area that we draw for borders later
	ZXColorView->PanelTopLeftPixel = ImGui::GetCursorScreenPos();
	ImGui::SetCursorPos(ImGui::GetCursorPos() + CalculatePanelSize(*ZXColorView));
	ZXColorView->ViewTopLeftPixel = ImGui::GetCursorScreenPos();
	const ImRect Rect(Window->DC.CursorPos, Window->DC.CursorPos + ZXColorView->ViewSize);

	// callback for using our own image shader 
	ImGui::GetWindowDrawList()->AddCallback(OnDrawCallback_ZXVideo, ZXColorView.get());
	ImGui::GetWindowDrawList()->AddImage(ZXColorView->Image.ShaderResourceView, Rect.Min, Rect.Max, ZXColorView->UV.Min, ZXColorView->UV.Max);
	
	if (ZXColorView->bVisibilityRectangleMarquee)
	{
		Draw_RectangleMarquee(ZXColorView, Rect);
	}
	// reset callback for using our own image shader 
	ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

bool UI::Draw_ButtonZXColorSprite(
	const char* StringID,
	std::shared_ptr<FSprite> Sprite,
	const ImVec2& VisibleSize,
	const FButtonZXColorSpriteSettings& Settings,
	bool* OutputHovered,
	const ImVec4& BackgroundColor,
	const ImVec4& TintColor,
	const ImVec4& SelectedColor)
{
	ImGuiWindow* Window = ImGui::GetCurrentWindow();
	if (Window->SkipItems)
	{
		return false;
	}

	const ImGuiID ID = Window->GetID(StringID);
	const ImGuiStyle& Style = ImGui::GetStyle();
	const ImVec2 LabelSize = ImGui::CalcTextSize(StringID, NULL, true);

	const ImVec2 Padding = Style.FramePadding;
	const ImRect Rect(Window->DC.CursorPos, Window->DC.CursorPos + VisibleSize + Padding * 2.0f);
	ImGui::ItemSize(Rect);
	if (!ImGui::ItemAdd(Rect, ID))
	{
		return false;
	}

	bool bHovered, bHeld;
	const bool bPressed = ImGui::ButtonBehavior(Rect, ID, &bHovered, &bHeld);
	if (OutputHovered != nullptr)
	{
		*OutputHovered = bHovered;
	}

	// show tooltip
	if (Settings.bVisibleTooltip)
	{
		if (bHovered)
		{
			Sprite->HoverStartTime = Sprite->HoverStartTime < 0.0f ? ImGui::GetTime() : Sprite->HoverStartTime;
		}
		else
		{
			Sprite->HoverStartTime = -1.0;
		}
		if (Sprite->HoverStartTime > 0.0f && ImGui::GetTime() - Sprite->HoverStartTime > 0.5f)
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted("Sprite info:");
			ImGui::Spacing();
			ImGui::TextUnformatted(std::format(" - size: {} x {}", Sprite->Width, Sprite->Height).c_str());
			if (Settings.bVisibleAdvencedTooltip)
			{
				ImGui::TextUnformatted(std::format(" - metadata: {}", Sprite->Regions.size() == 0 ? "none" : std::format("{}", Sprite->Regions.size()).c_str()).c_str());
				ImGui::TextUnformatted(std::format(" - selected: {}", Sprite->bSelected ? "true" : "false").c_str());
			}
			ImGui::EndTooltip();
		}
	}

	const float SpriteMin = ImMin((float)Sprite->Width, (float)Sprite->Height);
	const float SpriteMax = ImMax((float)Sprite->Width, (float)Sprite->Height);

	// render
	ImVec2 p0 = Rect.Min + Padding;
	ImVec2 p1 = Rect.Max - Padding;

	ImGui::RenderNavHighlight(Rect, ID);
	const ImU32 ButtonColor = Sprite->bSelected ? UI::ColorToU32(SelectedColor) : ImGui::GetColorU32((bHeld && bHovered && Settings.bHovered) ? ImGuiCol_ButtonActive : (bHovered && Settings.bHovered)? ImGuiCol_ButtonHovered : ImGuiCol_Button);
	ImGui::RenderFrame(Rect.Min, Rect.Max, ButtonColor, true, ImClamp((float)ImMin(Padding.x, Padding.y), 0.0f, Style.FrameRounding));
	if (BackgroundColor.w > 0.0f)
	{
		Window->DrawList->AddRectFilled(p0, p1, ImGui::GetColorU32(BackgroundColor));
	}

	const ImVec2 Scale = VisibleSize / SpriteMax;
	ImVec2 NewPadding((SpriteMax - Sprite->Width) * 0.5f, (SpriteMax - Sprite->Height) * 0.5f);
	p0 += NewPadding * Scale;
	p1 -= NewPadding * Scale;

	ImGui::PushClipRect(p0, p1, true);
	{
		const FImage& Image = Sprite->ZXColorView->Image;
		// callback for using our own image shader 
		ImGui::GetWindowDrawList()->AddCallback(UI::OnDrawCallback_ZXVideo, Sprite->ZXColorView.get());
		ImGui::GetWindowDrawList()->AddImage(
			Sprite->ZXColorView->Image.ShaderResourceView, 
			p0, p0 + Image.Size * Scale, 
			ImVec2(0.0f, 0.0f), ImVec2(1.0, 1.0f),
			ImGui::GetColorU32(TintColor));

		// reset callback for using our own image shader 
		ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
		
		for (const FButtonZXColorSpriteSettings::FLayer& Layer : Settings.Layers)
		{
			if (!Layer.ZXColorView)
			{
				continue;
			}
			std::shared_ptr<UI::FZXColorView> ZXColorView = Layer.ZXColorView;
			FImage& Image = ZXColorView->Image;

			ImGui::GetWindowDrawList()->AddCallback(UI::OnDrawCallback_ZXVideo, ZXColorView.get());
			ImGui::GetWindowDrawList()->AddImage(
				Image.ShaderResourceView,
				p0 + Layer.Offset * Scale,
				p0 + (Layer.Offset + Image.Size) * Scale,
				ImVec2(0.0f, 0.0f), ImVec2(1.0, 1.0f),
				ImGui::GetColorU32(Layer.TintColor));
		}
		// reset callback for using our own image shader 
		ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	}
	ImGui::PopClipRect();

	return bPressed;
}

void UI::Set_ZXViewPosition(std::shared_ptr<UI::FZXColorView> ZXColorView, ImVec2 NewPosition)
{
	ZXColorView->ImagePosition = NewPosition;
	RoundImagePosition(*ZXColorView);
}

void UI::Add_ZXViewDeltaPosition(std::shared_ptr<UI::FZXColorView> ZXColorView, ImVec2 DeltaPosition)
{
	ImVec2 uvDelta = DeltaPosition * ZXColorView->ViewSizeUV / ZXColorView->ViewSize;
	ZXColorView->ImagePosition -= uvDelta;
	RoundImagePosition(*ZXColorView);
}

Transform2D GetTexelsToPixels(const ImVec2& ScreenTopLeft, const ImVec2& ScreenViewSize, const ImVec2& UVTopLeft, const ImVec2& UVViewSize, const ImVec2& TextureSize)
{
	const ImVec2 UVToPixel = ScreenViewSize / UVViewSize;
	Transform2D Transform;
	Transform.Scale = UVToPixel / TextureSize;
	Transform.Translate.x = ScreenTopLeft.x - UVTopLeft.x * UVToPixel.x;
	Transform.Translate.y = ScreenTopLeft.y - UVTopLeft.y * UVToPixel.y;
	return Transform;
}

ImVec2 UI::GetMouse(std::shared_ptr<UI::FZXColorView> ZXColorView)
{
	Transform2D TexelsToPixels = GetTexelsToPixels(ZXColorView->ViewTopLeftPixel, ZXColorView->ViewSize, ZXColorView->UV.Min, ZXColorView->ViewSizeUV, ZXColorView->Image.Size);
	Transform2D PixelsToTexels = TexelsToPixels.Inverse();
	return PixelsToTexels * ImGui::GetMousePos();
}

void UI::Set_ZXViewScale(std::shared_ptr<UI::FZXColorView> ZXColorView, float Scale)
{
	// scale
	{
		ZXColorView->TexelsToPixels = GetTexelsToPixels(ZXColorView->ViewTopLeftPixel, ZXColorView->ViewSize, ZXColorView->UV.Min, ZXColorView->ViewSizeUV, ZXColorView->Image.Size);
		ZXColorView->PixelsToTexels = ZXColorView->TexelsToPixels.Inverse();

		const ImVec2 MousePosition = ImGui::GetMousePos();
		ImVec2 MousePositionTexel = ZXColorView->PixelsToTexels * MousePosition;
		const ImVec2 MouseUV = MousePositionTexel / ZXColorView->Image.Size;
		float LocalScale = ZXColorView->Scale.y;
		float PrevScale = LocalScale;

		bool keepTexelSizeRegular = LocalScale > ZXColorView->MinimumGridSize;
		if (Scale > 0.0f)
		{
			LocalScale *= ZXColorView->ZoomRate;
			if (keepTexelSizeRegular)
			{
				// it looks nicer when all the grid cells are the same size
				// so keep scale integer when zoomed in
				LocalScale = ImCeil(LocalScale);
			}
		}
		else
		{
			LocalScale /= ZXColorView->ZoomRate;
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
		SetScale(*ZXColorView, ImVec2(ZXColorView->PixelAspectRatio * LocalScale, LocalScale));
		if (LocalScale <= ZXColorView->ScaleMax.y && LocalScale >= ZXColorView->ScaleMin.y)
		{
			Set_ZXViewPosition(ZXColorView, ZXColorView->ImagePosition + (MouseUV - ZXColorView->ImagePosition) * (1.0f - PrevScale / LocalScale));
		}
	}
}

ImVec2 UI::ConverZXViewPositionToPixel(UI::FZXColorView& ZXColorView, const ImVec2& Position)
{
	const ImVec2 ImageSizeInv = ImVec2(1.0f, 1.0f) / ZXColorView.Image.Size;
	return ImFloor((Position - ZXColorView.ViewTopLeftPixel + ZXColorView.UV.Min / ImageSizeInv * ZXColorView.Scale) / ZXColorView.Scale);
}

void UI::ConvertZXIndexColorToDisplayRGB(FImage& InOutputImage, const std::vector<uint8_t> Data)
{
	if (!InOutputImage.IsValid())
	{
		return;
	}

	std::vector<uint32_t> RGBA(InOutputImage.GetLength());
	for (int32_t i = 0; i < Data.size(); ++i)
	{
		const uint8_t& Value = Data[i];
		const UI::EZXSpectrumColor::Type IndexColor = static_cast<UI::EZXSpectrumColor::Type>(Value);
		const uint32_t ColorRGBA = ToU32(UI::ZXSpectrumColorRGBA[IndexColor] | 0xFF);
		RGBA[i] = ColorRGBA;
	}

	FImageBase& Images = FImageBase::Get();
	Images.UpdateTexture(InOutputImage.Handle, RGBA.data());
}

void UI::GetInkPaper(const std::vector<uint8_t>& IndicesBoundary, uint8_t& OutputPaperColor, uint8_t& OutputInkColor, const UI::FConversationSettings& Settings)
{
	std::map<uint8_t, int32_t> Freq;
	for (uint8_t i : IndicesBoundary)
	{
		Freq[i]++;
	}

	std::vector<std::pair<uint8_t, int>> Sorted(Freq.begin(), Freq.end());
	std::sort(Sorted.begin(), Sorted.end(),
		[](auto& A, auto& B)
		{
			return A.second > B.second;
		});

	int32_t OftenEncountered = Sorted[0].first;
	int32_t SometimesEncountered = (Sorted.size() > 1) ? Sorted[1].first : OftenEncountered;

	if (Sorted.size() >= 3)
	{
		if (OftenEncountered == Settings.TransparentIndex)
		{
			OftenEncountered = SometimesEncountered;
			SometimesEncountered = Sorted[2].first;
		}
		else if (SometimesEncountered == Settings.TransparentIndex)
		{
			SometimesEncountered = Sorted[2].first;
		}

		// exceptions of colors with different brightness
		if ((OftenEncountered & 0x07) == (SometimesEncountered & 0x07))
		{
			SometimesEncountered = Sorted[2].first;
		}
	}

	if (Sorted.size() > 2)
	{
		if (OftenEncountered == Settings.TransparentIndex)
		{
			OftenEncountered = Settings.ReplaceTransparent;
		}
		if (SometimesEncountered == Settings.TransparentIndex)
		{
			SometimesEncountered = Settings.ReplaceTransparent;
		}
	}

	if (Sorted.size() == 1)
	{
		if (OftenEncountered == Settings.TransparentIndex)
		{
			OftenEncountered = Settings.InkAlways;
		}
		if (SometimesEncountered == Settings.TransparentIndex)
		{
			SometimesEncountered = Settings.ReplaceTransparent;
		}
	}
	
	OutputPaperColor = OftenEncountered;
	OutputInkColor = SometimesEncountered;
}

int32_t UI::FindClosestColor(ImU32 Color)
{
	int32_t Best = 0;
	int32_t BestDistance = INT_MAX;
	for (int32_t i = 0; i < IM_ARRAYSIZE(ZXSpectrumColorRGBA); ++i)
	{
		int32_t Distance_R = COLOR_R(Color) - COLOR_R(ZXSpectrumColorRGBA[i]);
		int32_t Distance_G = COLOR_G(Color) - COLOR_G(ZXSpectrumColorRGBA[i]);
		int32_t Distance_B = COLOR_B(Color) - COLOR_B(ZXSpectrumColorRGBA[i]);
		int32_t Distance_A = COLOR_A(Color) - COLOR_A(ZXSpectrumColorRGBA[i]);
		int32_t Distance = FMath::Square(Distance_R) + FMath::Square(Distance_G) + FMath::Square(Distance_B) + FMath::Square(Distance_A);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = i;
		}
	}
	return Best;
}

void UI::QuantizeToZX(uint8_t* RawImage, int32_t Width, int32_t Height, int32_t Channels, std::vector<uint8_t>& OutputIndexedData)
{
	const int32_t Size = Width * Height;
	OutputIndexedData.resize(Size);

	for (int i = 0; i < Size; ++i)
	{
		const uint8_t* Pixel = &RawImage[i * Channels];
		const ImU32 Color = COLOR(Pixel[0], Pixel[1], Pixel[2], Pixel[3]);
		const int8_t Index = FindClosestColor(Color);
		OutputIndexedData[i] = Index;
	}
}

void UI::ZXIndexColorToImage(FImage& InOutputImage, const std::vector<uint8_t>& IndexedData, int32_t Width, int32_t Height, bool bCreate)
{
	const int32_t Size = Width * Height;
	std::vector<uint32_t> RGBA(Size);
	for (int32_t i = 0; i < IndexedData.size(); ++i)
	{
		const uint8_t& Value = IndexedData[i];
		const UI::EZXSpectrumColor::Type IndexColor = static_cast<UI::EZXSpectrumColor::Type>(Value);
		const ImU32 ColorRGBA = ToU32(UI::ZXSpectrumColorRGBA[IndexColor]);
		RGBA[i] = ColorRGBA;
	}

	FImageBase& Images = FImageBase::Get();
	if (bCreate)
	{
		InOutputImage = Images.CreateTexture(RGBA.data(), Width, Height, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC);
	}
	else
	{
		Images.UpdateTexture(InOutputImage.Handle, RGBA.data());
	}
}

void UI::ZXIndexColorToZXAttributeColor(
	const std::vector<uint8_t>& IndexedData,
	int32_t Width, int32_t Height,
	std::vector<uint8_t>& OutputInkData,
	std::vector<uint8_t>& OutputAttributeData,
	std::vector<uint8_t>& OutputMaskData,
	const UI::FConversationSettings& Settings)
{
	const int32_t Boundary_X = Width >> 3;
	const int32_t Boundary_Y = Height >> 3;

	const int32_t PixelSize = Boundary_X * Height;
	OutputInkData.resize(PixelSize);
	OutputMaskData.resize(PixelSize);
	const int32_t AttributeSize = Boundary_X * Boundary_Y;
	OutputAttributeData.resize(AttributeSize);

	std::vector<uint8_t> Boundary(8 * 8);	
	for (int32_t y = 0; y < Boundary_Y; ++y)
	{
		for (int32_t x = 0; x < Boundary_X; ++x)
		{
			for (int32_t dy = 0; dy < 8; ++dy)
			{
				for (int32_t dx = 0; dx < 8; ++dx)
				{
					const int32_t BoundaryOffset = dy * 8 + dx;
					Boundary[BoundaryOffset] = IndexedData[(y * 8 + dy) * Width + (x * 8 + dx)];
				}
			}

			uint8_t PaperColor, InkColor;
			GetInkPaper(Boundary, PaperColor, InkColor, Settings);
			if (Settings.InkAlways == PaperColor)
			{
				std::swap(PaperColor, InkColor);
			}

			for (int32_t dy = 0; dy < 8; ++dy)
			{
				uint8_t Mask = 0;
				uint8_t PixelsInk = 0;
				for (int32_t dx = 0; dx < 8; ++dx)
				{
					Mask <<= 1;
					PixelsInk <<= 1;

					const int32_t BoundaryOffset = dy * 8 + dx;
					const uint8_t Index = Boundary[BoundaryOffset] & 0x07;
					if (Index == UI::EZXSpectrumColor::Transparent ? Settings.ReplaceTransparent == (InkColor & 0x07) : Index == (InkColor & 0x07))
					{
						PixelsInk |= 1;
					}

					if (Boundary[BoundaryOffset] == UI::EZXSpectrumColor::Transparent)
					{
						Mask |= 1;
					}
				}

				const int32_t PixelsOffset = (y * 8 + dy) * Boundary_X + x;
				OutputInkData[PixelsOffset] = PixelsInk;
				OutputMaskData[PixelsOffset] = ~Mask;
			}

			bool bBright = (InkColor & 0x08) && (PaperColor & 0x08);
			InkColor &= 0x07;
			PaperColor &= 0x07;

			uint8_t Attribut = (bBright << 6) | (PaperColor << 3) | InkColor;

			const int32_t Offset = y * Boundary_X + x;
			OutputAttributeData[Offset] = Attribut;
		}
	}
}

void UI::ZXAttributeColorToZXIndexColor(
	FImage& InOutputImage,
	int32_t Width, int32_t Height,
	std::vector<uint8_t>& OutputIndexedData,
	const std::vector<uint8_t>& InkData,
	const std::vector<uint8_t>& AttributeData,
	const std::vector<uint8_t>& MaskData)
{
	ZXAttributeColorToImage(
		InOutputImage,
		Width, Height,
		InkData.data(),
		AttributeData.data(),
		MaskData.data(),
		false,
		&OutputIndexedData);
}

void UI::ZXAttributeColorToImage(
	FImage& InOutputImage,
	int32_t Width, int32_t Height,
	const uint8_t* InkData,
	const uint8_t* AttributeData,
	const uint8_t* MaskData,
	bool bCreate /*= false*/,
	std::vector<uint8_t>* OutputIndexedData /*= nullptr*/)
{
	const int32_t Size = Width * Height;
	std::vector<uint32_t> RGBA(Size);

	if (OutputIndexedData != nullptr)
	{
		OutputIndexedData->resize(Size);
	}

	const int32_t Boundary_X = Width >> 3;
	const int32_t Boundary_Y = Height >> 3;

	bool bInk = InkData != nullptr;
	bool bMask = MaskData != nullptr;
	bool bAttribute = AttributeData != nullptr;

	for (int32_t i = 0; i < Size; ++i)
	{
		const int32_t y = i / Width;
		const int32_t x = i % Width;

		const int32_t bx = x / 8;
		const int32_t dx = x % 8;
		const int32_t by = y / 8;
		const int32_t dy = y % 8;

		uint8_t Mask = 0xFF;
		uint8_t Pixels = 0x00;
		uint8_t InkColor = EZXSpectrumColor::Black_;
		uint8_t PaperColor = EZXSpectrumColor::White_;

		if (bInk)
		{
			const int32_t InkOffset = (by * 8 + dy) * Boundary_X + bx;
			Pixels = InkData[InkOffset];
		}

		if (bMask)
		{
			const int32_t MaskOffset = (by * 8 + dy) * Boundary_X + bx;
			Mask = MaskData[MaskOffset];
		}

		if (bAttribute)
		{
			const int32_t AttributeOffset = by * Boundary_X + bx;
			const uint8_t Attribute = AttributeData[AttributeOffset];
			const bool bBright = (Attribute >> 6) & 0x01;
			InkColor = (Attribute & 0x07) | (bBright << 3);
			PaperColor = ((Attribute >> 3) & 0x07) | (bBright << 3);
		}

		if (InkColor == EZXSpectrumColor::Transparent)
		{
			InkColor = EZXSpectrumColor::Black_;
		}
		if (PaperColor == EZXSpectrumColor::Transparent)
		{
			PaperColor = EZXSpectrumColor::Black_;
		}

		if (bInk || bAttribute)
		{
			Mask = ~Mask;
		}
		else
		{
			std::swap(InkColor, PaperColor);
		}

		const int8_t ColorInk = (Pixels << dx) & 0x80 ? InkColor : PaperColor;
		const int8_t Color = ((Mask << dx) & 0x80) ? EZXSpectrumColor::Transparent : ColorInk;
		if (OutputIndexedData)
		{
			(*OutputIndexedData)[i] = Color;
		}
		const ImU32 ColorRGBA = ToU32(UI::ZXSpectrumColorRGBA[Color]);
		RGBA[i] = ColorRGBA;
	}

	FImageBase& Images = FImageBase::Get();
	if (bCreate)
	{
		InOutputImage = Images.CreateTexture(RGBA.data(), Width, Height, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC);
	}
	else
	{
		Images.UpdateTexture(InOutputImage.Handle, RGBA.data());
	}
}
