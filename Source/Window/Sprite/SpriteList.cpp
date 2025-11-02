#include "SpriteList.h"
#include <Utils/UI/Draw.h>
#include <Utils/UI/Draw_ZXColorVideo.h>
#include <Window/Common/FileDialog.h>
#include "resource.h"
#include "Events.h"
#include "Canvas.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Sprite List";
	const char* ExportToName = "Export##ExportTo";

	static constexpr ImVec2 VisibleSizeArray[] =
	{
		ImVec2(16.0f, 16.0f),
		ImVec2(24.0f, 24.0f),
		ImVec2(32.0f, 32.0f),
		ImVec2(48.0f, 48.0f),
		ImVec2(64.0f, 64.0f),
		ImVec2(96.0f, 96.0f),
		ImVec2(128.0f, 128.0f),
		ImVec2(256.0f, 256.0f),
		ImVec2(512.0f, 512.0f),
	};
}

SSpriteList::SSpriteList(EFont::Type _FontName, std::string _DockSlot /*= ""*/)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetDockSlot(_DockSlot)
		.SetIncludeInWindows(true))
	, bNeedKeptOpened_ExportPopup(false)
	, ScaleVisible(2)
{}

void SSpriteList::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_AddSprite>(
		[this](const FEvent_AddSprite& Event)
		{
			if (Event.Tag == FEventTag::AddSpriteTag)
			{
				AddSprite(
					Event.SpriteRect,
					Event.SpriteName,
					Event.Width,
					Event.Height,
					Event.IndexedData,
					Event.InkData,
					Event.AttributeData,
					Event.MaskData);
			}
		});
}

void SSpriteList::Initialize(const std::any& Arg)
{
	CurrentPath = CurrentPath.empty() ? std::filesystem::current_path().string() : CurrentPath;
}

void SSpriteList::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen);
	{
		Input_Mouse();

		float FooterHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("##Scrolling", ImVec2(0, -FooterHeight), false,
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoDecoration))
		{
			SpriteListID = ImGui::GetCurrentWindow()->ID;
			Draw_SpriteList();
		}
		ImGui::EndChild();
		ImGui::Separator();

		// button export
		{
			bool bEnabled = Sprites.size() > 0;
			if (bEnabled)
			{
				for (FSprite Sprite : Sprites)
				{
					bEnabled = Sprite.bSelected;
					if (bEnabled)
					{
						break;
					}
				}
			}
			if (!bEnabled) ImGui::BeginDisabled();

			const ImGuiID ConvertID = ImGui::GetCurrentWindow()->GetID(ExportToName);
			if (bNeedKeptOpened_ExportPopup || ImGui::Button("Export", ImVec2(0.0f, 0.0f)))
			{
				bNeedKeptOpened_ExportPopup = false;
				ImGui::OpenPopup(ConvertID);
			}
			if (ImGui::BeginPopupModal(ExportToName, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
			{
				Draw_ExportSprites();
				/*ImGui::EndPopup();*/
			}
			if (!bEnabled) ImGui::EndDisabled();
			ImGui::SameLine();
		}

		// draw current scale
		{
			ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_WEAK));
			UI::TextAligned(std::format("x{}", VisibleSizeArray[ScaleVisible].x).c_str(), { 1.0f, 0.5f });
			ImGui::PopStyleColor();
		}
		ImGui::End();
	}
}

void SSpriteList::Input_Mouse()
{
	ImGuiContext& Context = *ImGui::GetCurrentContext();
	const bool bHovered = ImGui::IsWindowHovered();
	const float MouseWheel = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner) ? Context.IO.MouseWheel : 0.0f;
	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != SpriteListID)
	{
		return;
	}

	if (MouseWheel != 0.0f && Context.IO.KeyCtrl && !Context.IO.FontAllowUserScaling)
	{
		ScaleVisible = (uint32_t)ImClamp<float>(ScaleVisible + MouseWheel, 0.0f, IM_ARRAYSIZE(VisibleSizeArray) - 1);
	}

	if (!ImGui::IsWindowFocused())
	{
		return;
	}
}

void SSpriteList::Draw_SpriteList()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(50.0f, 65.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 2.0f));

	const ImVec4 BackgroundColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	const ImVec4 TintColor = ImVec4(1.0f, 1.0f, 0.0f, 0.5f);
	const ImVec4 SelectedColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

	const ImGuiStyle& Style = ImGui::GetStyle();
	const ImVec2 VisibleSize = VisibleSizeArray[ScaleVisible];
	const float WindowVisible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

	const uint32_t SpriteNum = uint32_t(Sprites.size());
	for (uint32_t Index = 0; Index < SpriteNum; ++Index)
	{
		ImGui::PushID(Index);

		FSprite& Sprite = Sprites[Index];
		const std::string StringID = std::format("SpriteButton##%{}", Index);

		ImGui::BeginGroup();
		if (Draw_ButtonSprite(StringID.c_str(), Sprite, VisibleSize, BackgroundColor, TintColor, SelectedColor))
		{
			Sprite.bSelected = !Sprite.bSelected;
		}
		ImGui::Text(Sprite.Name.c_str());
		ImGui::EndGroup();

		const float LastButton_x2 = ImGui::GetItemRectMax().x;
		const float NextButton_x2 = LastButton_x2 + Style.ItemSpacing.x + VisibleSize.x; // Expected position if next button was on same line
		if (Index + 1 < SpriteNum && NextButton_x2 < WindowVisible_x2)
		{
			ImGui::SameLine();
		}
		ImGui::PopID();
	}

	ImGui::PopStyleVar(3);
}

bool SSpriteList::Draw_ButtonSprite(const char* StringID, FSprite& Sprite, const ImVec2& VisibleSize, const ImVec4& BackgroundColor, const ImVec4& TintColor, const ImVec4& SelectedColor)
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

	// show tooltip
	{
		if (bHovered)
		{
			Sprite.HoverStartTime = Sprite.HoverStartTime < 0.0f ? ImGui::GetTime() : Sprite.HoverStartTime;
		}
		else
		{
			Sprite.HoverStartTime = -1.0;
		}
		if (Sprite.HoverStartTime > 0.0f && ImGui::GetTime() - Sprite.HoverStartTime > 0.5f)
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted("Sprite info:");
			ImGui::Spacing();
			ImGui::TextUnformatted(std::format(" - size: {} x {}", Sprite.Width, Sprite.Height).c_str());
			ImGui::TextUnformatted(std::format(" - selected: {}", Sprite.bSelected ? "true" : "false").c_str());
			ImGui::EndTooltip();
		}
	}

	const float SpriteMin = ImMin((float)Sprite.Width, (float)Sprite.Height);
	const float SpriteMax = ImMax((float)Sprite.Width, (float)Sprite.Height);

	// render
	ImVec2 p0 = Rect.Min + Padding;
	ImVec2 p1 = Rect.Max - Padding;

	ImGui::RenderNavHighlight(Rect, ID);
	const ImU32 ButtonColor = Sprite.bSelected ? UI::ColorToU32(SelectedColor) : ImGui::GetColorU32((bHeld && bHovered) ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
	ImGui::RenderFrame(Rect.Min, Rect.Max, ButtonColor, true, ImClamp((float)ImMin(Padding.x, Padding.y), 0.0f, Style.FrameRounding));
	if (BackgroundColor.w > 0.0f)
	{
		Window->DrawList->AddRectFilled(p0, p1, ImGui::GetColorU32(BackgroundColor));
	}

	const ImVec2 Scale = VisibleSize / SpriteMax;
	ImVec2 NewPadding((SpriteMax - Sprite.Width) * 0.5f, (SpriteMax - Sprite.Height) * 0.5f);
	p0 += NewPadding * Scale;
	p1 -= NewPadding * Scale;

	ImGui::PushClipRect(p0, p1, true);
	{
		const FImage& Image = Sprite.ZXColorView->Image;
		// callback for using our own image shader 
		ImGui::GetWindowDrawList()->AddCallback(UI::OnDrawCallback_ZXVideo, Sprite.ZXColorView.get());
		ImGui::GetWindowDrawList()->AddImage(Sprite.ZXColorView->Image.ShaderResourceView, p0, p0 + Image.Size * Scale, ImVec2(0.0f, 0.0f), ImVec2(1.0, 1.0f), ImGui::GetColorU32(TintColor));
		// reset callback for using our own image shader 
		ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	}
	ImGui::PopClipRect();

	return bPressed;
}

void SSpriteList::Draw_ExportSprites()
{
	static bool bOpenFileDialog = false;
	const float TextHeight = ImGui::GetTextLineHeightWithSpacing(); 
	ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
	ImGui::Text("%s", CurrentPath.c_str());
	ImGui::SameLine(450.0f);
	if (ImGui::Button("...", ImVec2(0.0f, 0.0f)))
	{
		bOpenFileDialog = true;
		ImGui::CloseCurrentPopup();
	}
	ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
	if (ImGui::Button("Cancel", ImVec2(0.0f, 0.0f)))
	{
		bNeedKeptOpened_ExportPopup = false;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();

	if (bOpenFileDialog)
	{
		bOpenFileDialog = false;
		SFileDialog::OpenWindow(GetParent(), "Select File", EDialogMode::Select,
			[this](std::filesystem::path SelectedPath, EDialogStage Selected) -> void
			{
				if (Selected == EDialogStage::Selected)
				{
					CurrentPath = SelectedPath.string();
				}
				SFileDialog::CloseWindow();
				bNeedKeptOpened_ExportPopup = true;
			}, CurrentPath, "*.*");
	}
}

void SSpriteList::AddSprite(
	const ImRect& SpriteRect,
	const std::string& Name,
	int32_t Width, int32_t Height,
	const std::vector<uint8_t>& IndexedData,
	const std::vector<uint8_t>& InkData,
	const std::vector<uint8_t>& AttributeData,
	const std::vector<uint8_t>& MaskData)
{
	FSprite NewSprite;
	NewSprite.bSelected = false;
	NewSprite.HoverStartTime = -1.0;
	NewSprite.Width = (uint32_t)SpriteRect.GetWidth();
	NewSprite.Height = (uint32_t)SpriteRect.GetHeight();
	NewSprite.Name = Name;
	NewSprite.ZXColorView = std::make_shared<UI::FZXColorView>();
	NewSprite.ZXColorView->Scale = ImVec2(1.0f, 1.0f);
	NewSprite.ZXColorView->ImagePosition = ImVec2(0.0f, 0.0f);

	{
		const uint32_t NormalSizeX = uint32_t(NewSprite.Width / 8) * 8;
		const uint32_t NormalSizeY = uint32_t(NewSprite.Height / 8) * 8;

		if (NormalSizeX != NewSprite.Width || NormalSizeY != NewSprite.Height)
		{
			// ToDo: convert index color to ZX format
			return;
		}

		const int32_t Size = NewSprite.Width * NewSprite.Height;
		std::vector<uint32_t> RGBA(Size);

		std::vector<uint8_t>& NewIndexedData = NewSprite.ZXColorView->IndexedData;
		NewIndexedData.resize(Size);

		const int32_t Boundary_X = Width >> 3;
		const int32_t Boundary_Y = Height >> 3;

		const int32_t PixelSize = Boundary_X * NewSprite.Height;
		std::vector<uint8_t>& NewInkData = NewSprite.ZXColorView->InkData;
		NewInkData.resize(PixelSize);
		std::vector<uint8_t>& NewMaskData = NewSprite.ZXColorView->MaskData;
		NewMaskData.resize(PixelSize);

		const int32_t AttributeSize = Boundary_X * Boundary_Y;
		std::vector<uint8_t>& NewAttributeData = NewSprite.ZXColorView->AttributeData;
		NewAttributeData.resize(AttributeSize);

		uint32_t Index = 0;
		for (uint32_t y = (uint32_t)SpriteRect.Min.y; y < (uint32_t)SpriteRect.Max.y; ++y)
		{
			for (uint32_t x = (uint32_t)SpriteRect.Min.x; x < (uint32_t)SpriteRect.Max.x; ++x)
			{
				const int32_t bx = x / 8;
				const int32_t dx = x % 8;
				const int32_t by = y / 8;
				const int32_t dy = y % 8;

				uint8_t Mask = 0xFF;
				uint8_t Pixels = 0x00;
				uint8_t InkColor = UI::EZXSpectrumColor::Black_;
				uint8_t PaperColor = UI::EZXSpectrumColor::White_;

				//if (bInk)
				{
					const int32_t InkOffset = (by * 8 + dy) * Boundary_X + bx;
					Pixels = InkData[InkOffset];
				}

				//if (bMask)
				{
					const int32_t MaskOffset = (by * 8 + dy) * Boundary_X + bx;
					Mask = MaskData[MaskOffset];
				}

				//if (bAttribute)
				{
					const int32_t AttributeOffset = by * Boundary_X + bx;
					const uint8_t Attribute = AttributeData[AttributeOffset];
					const bool bBright = (Attribute >> 6) & 0x01;
					InkColor = (Attribute & 0x07) | (bBright << 3);
					PaperColor = ((Attribute >> 3) & 0x07) | (bBright << 3);
				}

				if (InkColor == UI::EZXSpectrumColor::Transparent)
				{
					InkColor = UI::EZXSpectrumColor::Black_;
				}
				if (PaperColor == UI::EZXSpectrumColor::Transparent)
				{
					PaperColor = UI::EZXSpectrumColor::Black_;
				}

				//if (bInk || bAttribute)
				//{
					Mask = ~Mask;
				//}
				//else
				//{
				//	std::swap(InkColor, PaperColor);
				//}

				const int8_t ColorInk = (Pixels << dx) & 0x80 ? InkColor : PaperColor;
				const int8_t Color = ((Mask << dx) & 0x80) ? UI::EZXSpectrumColor::Transparent : ColorInk;
				//if (OutputIndexedData)
				{
					NewIndexedData[Index] = Color;
				}
				const ImU32 ColorRGBA = UI::ToU32(UI::ZXSpectrumColorRGBA[Color]);
				RGBA[Index] = ColorRGBA;

				++Index;
			}
		}

		FImageBase& Images = FImageBase::Get();
		NewSprite.ZXColorView->Image = Images.CreateTexture(RGBA.data(), NewSprite.Width, NewSprite.Height, D3D11_CPU_ACCESS_READ, D3D11_USAGE_DEFAULT);
	}


	//UI::ZXIndexColorToImage(NewSprite.ZXColorView->Image, NewSprite.ZXColorView->IndexedData, NewSprite.Width, NewSprite.Height, true);
	
	NewSprite.ZXColorView->Device = Data.Device;
	NewSprite.ZXColorView->DeviceContext = Data.DeviceContext;
	Draw_ZXColorView_Initialize(NewSprite.ZXColorView, UI::ERenderType::Sprite);

	Sprites.push_back(NewSprite);
}
