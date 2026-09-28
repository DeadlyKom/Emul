#include "Palette.h"
#include <Utils/UI/Draw.h>
#include <Utils/UI/Draw_ZXColorVideo.h>
#include <Window/Sprite/Events.h>
#include "Canvas.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Palette";
}

SPalette::SPalette(EFont::Type _FontName, std::string _DockSlot /*= ""*/)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetDockSlot(_DockSlot)
		.SetIncludeInWindows(true))
	, OptionsFlags(FCanvasOptionsFlags::Source)
{
	ButtonColor[0] = UI::EZXSpectrumColor::Black_;	// left button
	ButtonColor[1] = UI::EZXSpectrumColor::White_;	// right button

	Subcolor[ESubcolor::Ink] = UI::EZXSpectrumColor::Black_;
	Subcolor[ESubcolor::Paper] = UI::EZXSpectrumColor::Black;
	Subcolor[ESubcolor::Bright] = UI::EZXSpectrumColor::False;
	Subcolor[ESubcolor::Flash] = UI::EZXSpectrumColor::False;

	// Initialize independent Ink and Paper selections for both mouse buttons.
	for (uint8_t ButtonIndex = 0; ButtonIndex < 2; ++ButtonIndex)
	{
		ButtonSubcolor[ButtonIndex][ESubcolor::Ink] = Subcolor[ESubcolor::Ink];
		ButtonSubcolor[ButtonIndex][ESubcolor::Paper] = Subcolor[ESubcolor::Paper];
	}
}

void SPalette::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_Canvas>(
		[this](const FEvent_Canvas& Event)
		{
			if (Event.Tag == FEventTag::CanvasOptionsFlagsTag)
			{
				OptionsFlags = Event.OptionsFlags;
			}
		});
	SubscribeEvent<FEvent_Color>(
		[this](const FEvent_Color& Event)
		{
			Event.ButtonIndex;				// pressed mouse button
			Event.SelectedColorIndex;		// zx color
			Event.SelectedSubcolorIndex;	// type ink/paper/bright

			// Update only the selected button's pixel operation.
			if (Event.Tag == FEventTag::ChangePixelOperationTag && Event.ButtonIndex < 2)
			{
				ButtonPixelOperation[Event.ButtonIndex] = Event.PixelOperation;
			}

			if (Event.Tag == FEventTag::ChangeColorTag)
			{
				if (Event.SelectedSubcolorIndex == ESubcolor::All)
				{
					uint8_t Attribute = Event.SelectedColorIndex;
					const bool bAttributeBright = (Attribute >> 6) & 0x01;
					const uint8_t AttributeInkColor = (Attribute & 0x07);
					const uint8_t AttributePaperColor = ((Attribute >> 3) & 0x07);

					Subcolor[ESubcolor::Ink] = (UI::EZXSpectrumColor::Type)AttributeInkColor;
					Subcolor[ESubcolor::Paper] = (UI::EZXSpectrumColor::Type)AttributePaperColor;
					Subcolor[ESubcolor::Bright] = bAttributeBright ? UI::EZXSpectrumColor::True : UI::EZXSpectrumColor::False;

					if (Subcolor[ESubcolor::Ink] == EZXColor::Transparent)
					{
						Subcolor[ESubcolor::Ink] = EZXColor::Black_;
					}
					if (Subcolor[ESubcolor::Paper] == EZXColor::Transparent)
					{
						Subcolor[ESubcolor::Paper] = EZXColor::Black_;
					}

					// Apply the sampled Ink and Paper only to the pressed mouse button preview.
					if (Event.ButtonIndex < 2)
					{
						ButtonSubcolor[Event.ButtonIndex][ESubcolor::Ink] = Subcolor[ESubcolor::Ink];
						ButtonSubcolor[Event.ButtonIndex][ESubcolor::Paper] = Subcolor[ESubcolor::Paper];
					}
				}
				else
				{
					// Store each button's Ink and Paper without replacing its other component.
					if (Event.ButtonIndex < 2)
					{
						// Keep Bright and Flash shared between both buttons.
						if (Event.SelectedSubcolorIndex <= ESubcolor::Paper)
						{
							ButtonColor[Event.ButtonIndex] = Event.SelectedColorIndex;
							ButtonSubcolor[Event.ButtonIndex][Event.SelectedSubcolorIndex] = Event.SelectedColorIndex;
						}
					}
					if (Event.SelectedSubcolorIndex < ESubcolor::MAX)
					{
						Subcolor[Event.SelectedSubcolorIndex] = Event.SelectedColorIndex;
					}
				}
			}
		});
}

void SPalette::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen);
	{
		Display_Colors();
		ImGui::End();
	}
}

void SPalette::Destroy()
{
	UnsubscribeAll();
}

void SPalette::Display_Colors()
{
	const float ColorBox = 50.0f;
	const float PaletteBox = 100.0f;
	const float PreviewSize = 18.0f;
	const ImVec2 StartPosition = ImGui::GetCursorPos();
	if (OptionsFlags & FCanvasOptionsFlags::Source)
	{
		// Match the rounded previews used in IPM mode.
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
		ImGui::Text("Left");
		ImGui::SameLine();
		ImGui::SetCursorPosX(ColorBox);
		ImGui::ColorButton(TEXT("LeftButton##Color"),
			UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[0]]),
			ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(PreviewSize, PreviewSize));

		// Align the second palette row with the larger Right preview.
		const float RightPreviewY = ImGui::GetCursorPosY();
		ImGui::Text("Right");
		ImGui::SameLine();
		ImGui::SetCursorPosX(ColorBox);
		ImGui::ColorButton(TEXT("RightButton##Color"),
			UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[1]]),
			ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(PreviewSize, PreviewSize));
		ImGui::PopStyleVar();

		ImGui::SetCursorPos(StartPosition + ImVec2(PaletteBox, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.4f, 4.4f));
		for (int32_t i = 0; i < IM_ARRAYSIZE(UI::ZXSpectrumColorRGBA); ++i) {
			// Mark both mouse buttons' selected source colors.
			const bool bIsSelected = (ButtonColor[0] == i || ButtonColor[1] == i);
			const std::string ButtonName = std::format(TEXT("IndexButton##Color {}"), i);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, bIsSelected ? 0.0f : 100.0f);

			const ImGuiColorEditFlags Flags = (!bIsSelected ? ImGuiColorEditFlags_NoBorder : ImGuiColorEditFlags_None) |
				ImGuiColorEditFlags_AlphaPreview |
				ImGuiColorEditFlags_NoTooltip |
				ImGuiColorEditFlags_NoDragDrop;
			uint8_t ButtonPressed = -1;
			if (UI::ColorButton(ButtonName.c_str(),
				ButtonPressed,
				UI::ToVec4(UI::ZXSpectrumColorRGBA[i]),
				Flags,
				ImGuiButtonFlags_MouseButtonLeft |
				ImGuiButtonFlags_MouseButtonRight |
				ImGuiButtonFlags_PressedOnClick,
				ImVec2(16, 16)))
			{
				Subcolor[ESubcolor::Ink] = i;

				if (ButtonPressed < 2)
				{
					ButtonColor[ButtonPressed] = i;
					ButtonSubcolor[ButtonPressed][ButtonPressed] = i;

					FEvent_Color Event;
					{
						Event.Tag = FEventTag::ChangeColorTag;
						Event.ButtonIndex = ButtonPressed;								// pressed mouse button
						Event.SelectedColorIndex = (UI::EZXSpectrumColor::Type)i;		// zx color
						Event.SelectedSubcolorIndex = (ESubcolor::Type)ButtonPressed;	// ink (LKM), paper (RKM)
					}
					SendEvent(Event);
				}
			}

			if ((i + 1) % 32 != 0 && i != 8 - 1)
			{
				ImGui::SameLine();
			}
			else
			{
				ImGui::SetCursorPos(ImVec2(StartPosition.x + PaletteBox, RightPreviewY));
			}
			ImGui::PopStyleVar();
		}
		ImGui::PopStyleVar();
	}
	else
	{
		static constexpr EZXColor IPColor[] = {
			UI::EZXSpectrumColor::Black,
			UI::EZXSpectrumColor::Black_,
			UI::EZXSpectrumColor::Blue,
			UI::EZXSpectrumColor::Red,
			UI::EZXSpectrumColor::Magenta,
			UI::EZXSpectrumColor::Green,
			UI::EZXSpectrumColor::Cyan,
			UI::EZXSpectrumColor::Yellow,
			UI::EZXSpectrumColor::White,
		};
		static constexpr EZXColor IColor[] = {
			UI::EZXSpectrumColor::Black_,
			UI::EZXSpectrumColor::White,
		};
		static constexpr EZXColor MColor[] = {
			UI::EZXSpectrumColor::Black,
			UI::EZXSpectrumColor::Black_,
		};
		static constexpr EZXColor IMColor[] = {
			UI::EZXSpectrumColor::Black,
			UI::EZXSpectrumColor::Black_,
			UI::EZXSpectrumColor::White,
		};
		static constexpr EZXColor BrightColor[] = {
			UI::EZXSpectrumColor::Transparent,
			UI::EZXSpectrumColor::White,
			UI::EZXSpectrumColor::White_,
		};
		static constexpr EZXColor BrightSelect[] = {
			UI::EZXSpectrumColor::Transparent,
			UI::EZXSpectrumColor::False,
			UI::EZXSpectrumColor::True,
		};

		const EZXColor* InkColorArray = IPColor;
		const EZXColor* PaperColorArray = IPColor;
		const EZXColor* BrightColorArray = BrightColor;

		bool bLRButton = false;
		int32_t InkColorSize = IM_ARRAYSIZE(IPColor);
		int32_t PaperColorSize = IM_ARRAYSIZE(IPColor);
		int32_t BrightColorSize = IM_ARRAYSIZE(BrightColor);

		const uint8_t Flags = OptionsFlags & ~FCanvasOptionsFlags::Source;
		switch (Flags)
		{
		case FCanvasOptionsFlags::Ink:																// 0010
			InkColorArray = IColor;
			InkColorSize = IM_ARRAYSIZE(IColor);
			PaperColorArray = nullptr;
			BrightColorArray = nullptr;

			bLRButton = true;
			break;
		case FCanvasOptionsFlags::Attribute:														// 0100
		case FCanvasOptionsFlags::Ink | FCanvasOptionsFlags::Attribute:								// 0110
			InkColorArray = IPColor;
			InkColorSize = IM_ARRAYSIZE(IPColor);
			PaperColorArray = InkColorArray;
			PaperColorSize = InkColorSize;
			break;
		case FCanvasOptionsFlags::Mask:																// 1000
			InkColorArray = MColor;
			InkColorSize = IM_ARRAYSIZE(MColor);
			PaperColorArray = nullptr;
			BrightColorArray = nullptr;

			bLRButton = true;
			break;

		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Ink:									// 1010
			InkColorArray = IMColor;
			InkColorSize = IM_ARRAYSIZE(IMColor);
			PaperColorArray = nullptr;
			BrightColorArray = nullptr;

			bLRButton = true;
			break;
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Attribute:							// 1100
			break;
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Attribute | FCanvasOptionsFlags::Ink:	// 1110
			break;
		}

		const bool bBright = Subcolor[ESubcolor::Bright] == UI::EZXSpectrumColor::True;
		const bool bFlash = Subcolor[ESubcolor::Flash] == UI::EZXSpectrumColor::True;

		// Show a checkerboard when brightness should be preserved while drawing.
		const uint8_t Bright = Subcolor[ESubcolor::Bright] == EZXColor::Transparent ? EZXColor::Transparent : BrightColor[1 + bBright];

		auto DrawButtonColorsLambda = [this, PreviewSize](const char* ButtonName, ESubcolor::Type SubColorIndex)
			{
				ImU32 Colors[2];
				for (uint8_t ButtonIndex = 0; ButtonIndex < 2; ++ButtonIndex)
				{
					// Read this button's saved component and the shared brightness setting.
					const uint8_t Color = ButtonSubcolor[ButtonIndex][SubColorIndex];
					const uint8_t Bright = Subcolor[ESubcolor::Bright];
					uint8_t ColorIndex = EZXColor::Transparent;

					// Keep transparent selections separate from opaque black.
					if (Color != EZXColor::Transparent)
					{
						// Compose the visible color with the button's brightness setting.
						ColorIndex = (Color & 0x07) | ((Bright == EZXColor::True) << 3);
						ColorIndex = ColorIndex == EZXColor::Black ? EZXColor::Black_ : ColorIndex;
					}
					Colors[ButtonIndex] = ImGui::GetColorU32(UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorIndex]));
				}

				// Draw the checkerboard underneath either transparent triangle.
				ImGui::ColorButton(ButtonName,
					UI::ToVec4(UI::ZXSpectrumColorRGBA[EZXColor::Transparent]),
					ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(PreviewSize, PreviewSize));

				// Split the rounded preview along the bottom-left to top-right diagonal.
				const ImVec2 Min = ImGui::GetItemRectMin() + ImVec2(0.75f, 0.75f);
				const ImVec2 Max = ImGui::GetItemRectMax() - ImVec2(0.75f, 0.75f);
				const float Rounding = 3.0f;
				ImDrawList* DrawList = ImGui::GetWindowDrawList();
				// Follow the rounded outer corners of the left-button half.
				DrawList->PathArcTo(ImVec2(Min.x + Rounding, Max.y - Rounding), Rounding, IM_PI * 0.75f, IM_PI);
				DrawList->PathArcTo(Min + ImVec2(Rounding, Rounding), Rounding, IM_PI, IM_PI * 1.5f);
				DrawList->PathArcTo(ImVec2(Max.x - Rounding, Min.y + Rounding), Rounding, IM_PI * 1.5f, IM_PI * 1.75f);
				DrawList->PathFillConvex(Colors[0]);
				// Follow the rounded outer corners of the right-button half.
				DrawList->PathArcTo(ImVec2(Max.x - Rounding, Min.y + Rounding), Rounding, IM_PI * 1.75f, IM_PI * 2.0f);
				DrawList->PathArcTo(Max - ImVec2(Rounding, Rounding), Rounding, 0.0f, IM_PI * 0.5f);
				DrawList->PathArcTo(ImVec2(Min.x + Rounding, Max.y - Rounding), Rounding, IM_PI * 0.5f, IM_PI * 0.75f);
				DrawList->PathFillConvex(Colors[1]);
				// End the separator at the rounded outline.
				const float DiagonalInset = Rounding * (1.0f - 0.70710678f);
				const ImVec2 TopRight(Max.x - DiagonalInset, Min.y + DiagonalInset);
				const ImVec2 BottomLeft(Min.x + DiagonalInset, Max.y - DiagonalInset);
				DrawList->AddLine(BottomLeft, TopRight, ImGui::GetColorU32(ImGuiCol_Border));
			};

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
		if (!bLRButton)
		{
			ImGui::Text("Ink");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			DrawButtonColorsLambda(TEXT("InkButton##Color"), ESubcolor::Ink);
		}
		else
		{
			ImGui::Text("Left");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			ImGui::ColorButton(TEXT("LeftButton##Color"),
				UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[0]]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(PreviewSize, PreviewSize));
		}

		// Align the right-button operation with the Paper or Right preview row.
		const float RightOperationY = ImGui::GetCursorPosY();
		if (PaperColorArray)
		{
			ImGui::Text("Paper");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			DrawButtonColorsLambda(TEXT("PaperButton##Color"), ESubcolor::Paper);
		}
		else if (bLRButton)
		{
			ImGui::Text("Right");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			ImGui::ColorButton(TEXT("RightButton##Color"),
				UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[1]]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(PreviewSize, PreviewSize));
		}
		// Keep the Bright swatches aligned with their larger preview.
		const float BrightRowY = ImGui::GetCursorPosY();
		if (BrightColorArray)
		{
			ImGui::Text("Bright");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			ImGui::ColorButton(TEXT("BrightButton##Color"),
				UI::ToVec4(UI::ZXSpectrumColorRGBA[Bright]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(PreviewSize, PreviewSize));
		}
		ImGui::PopStyleVar();

		auto ColorButtonsLambda = [=, this](ESubcolor::Type SubColorIndex, const EZXColor* ColorArray, int32_t Size, const EZXColor* SelectArray)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.4f, 4.4f));
				for (int32_t i = 0; i < Size; ++i) {
					// Compare the saved color with this entry's value, not its palette position.
					const EZXColor SelectValue = SelectArray != nullptr ? SelectArray[i] : ColorArray[i];
					// Mark both buttons for Ink and Paper while Bright and Flash remain shared.
					const bool bIsSelected = SubColorIndex <= ESubcolor::Paper
						? (ButtonSubcolor[0][SubColorIndex] == SelectValue || ButtonSubcolor[1][SubColorIndex] == SelectValue)
						: (Subcolor[SubColorIndex] == SelectValue);
					const std::string ButtonName = std::format(TEXT("IndexButton##Color {}x{}"), (int32_t)SubColorIndex, i);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, bIsSelected ? 0.0f : 100.0f);

					const ImGuiColorEditFlags Flags = 
						(!bIsSelected ? ImGuiColorEditFlags_NoBorder : ImGuiColorEditFlags_None) |
						ImGuiColorEditFlags_AlphaPreview |
						ImGuiColorEditFlags_NoTooltip |
						ImGuiColorEditFlags_NoDragDrop;
					uint8_t ButtonPressed = -1;
					if (UI::ColorButton(ButtonName.c_str(), 
						ButtonPressed, 
						UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorArray[i]]),
						Flags,
						ImGuiButtonFlags_MouseButtonLeft |
						ImGuiButtonFlags_MouseButtonRight |
						ImGuiButtonFlags_PressedOnClick,
						ImVec2(16, 16)))
					{
						Subcolor[SubColorIndex] = SelectValue;

						if (ButtonPressed < 2)
						{
							// Keep shared brightness settings out of the mouse button colors.
							if (SubColorIndex <= ESubcolor::Paper)
							{
								ButtonColor[ButtonPressed] = SelectValue;
								ButtonSubcolor[ButtonPressed][SubColorIndex] = SelectValue;
							}

							FEvent_Color Event;
							{
								Event.Tag = FEventTag::ChangeColorTag;
								Event.ButtonIndex = ButtonPressed;				// pressed mouse button
								Event.SelectedColorIndex = SelectValue;			// zx color
								Event.SelectedSubcolorIndex = SubColorIndex;	// type ink/paper/bright
							}
							SendEvent(Event);
						}
					}

					// Keep dark colors visible and highlight the swatch under the pointer.
					const bool bHovered = ImGui::IsItemHovered();
					const ImU32 BorderColor = ImGui::GetColorU32(bHovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
					// Fit the outline inside the swatch and follow its existing rounded shape.
					const float BorderThickness = bHovered ? 2.0f : 1.0f;
					const ImVec2 BorderInset(BorderThickness * 0.5f, BorderThickness * 0.5f);
					const float BorderRounding = ImMin(ImGui::GetStyle().FrameRounding, 16.0f / 2.99f * 0.5f);
					ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() + BorderInset, ImGui::GetItemRectMax() - BorderInset,
						BorderColor, BorderRounding, ImDrawFlags_None, BorderThickness);

					if ((i + 1) % 32 != 0 && i != Size - 1)
					{
						ImGui::SameLine();
					}
					ImGui::PopStyleVar();
				}
				ImGui::PopStyleVar();
			};		

		ImGui::SetCursorPos(StartPosition + ImVec2(PaletteBox, 0.0f));
		ColorButtonsLambda(ESubcolor::Ink, InkColorArray, InkColorSize, nullptr);
		if (PaperColorArray)
		{
			ImGui::SetCursorPos(ImVec2(StartPosition.x + PaletteBox, RightOperationY));
			ColorButtonsLambda(ESubcolor::Paper, PaperColorArray, PaperColorSize, nullptr);
		}
		if (BrightColorArray)
		{
			ImGui::SetCursorPos(ImVec2(StartPosition.x + PaletteBox, BrightRowY));
			ColorButtonsLambda(ESubcolor::Bright, BrightColorArray, BrightColorSize, BrightSelect);
		}

		// Place both operation selectors to the right of the longest palette row.
		const float OperationX = StartPosition.x + PaletteBox + ImMax(InkColorSize, PaperColorArray ? PaperColorSize : 0) * (16.0f + 2.4f) + 8.0f;
		// Use one selector column and center its text within the preview height.
		const float OperationComboX = OperationX + ImMax(ImGui::CalcTextSize("Left").x, ImGui::CalcTextSize("Right").x) + ImGui::GetStyle().ItemInnerSpacing.x;
		const float OperationPaddingY = ImMax(0.0f, (PreviewSize - ImGui::GetFontSize()) * 0.5f);
		auto PixelOperationLambda = [this, OperationX, OperationComboX, OperationPaddingY](const char* Label, uint8_t ButtonIndex, float RowY)
			{
				static const char* OperationNames[] = { "SET", "RES", "XOR", "None" };
				int32_t OperationIndex = ButtonPixelOperation[ButtonIndex];
				ImGui::SetCursorPos(ImVec2(OperationX, RowY + OperationPaddingY));
				ImGui::TextUnformatted(Label);
				ImGui::SetCursorPos(ImVec2(OperationComboX, RowY));
				ImGui::PushID(ButtonIndex);
				ImGui::SetNextItemWidth(70.0f);
				// Apply a changed operation only to this mouse button.
				if (ImGui::Combo("##PixelOperation", &OperationIndex, OperationNames, IM_ARRAYSIZE(OperationNames)))
				{
					ButtonPixelOperation[ButtonIndex] = (EPixelOperation::Type)OperationIndex;
					// Publish the operation independently of the selected Ink and Paper colors.
					FEvent_Color Event;
					Event.Tag = FEventTag::ChangePixelOperationTag;
					Event.ButtonIndex = ButtonIndex;
					Event.PixelOperation = ButtonPixelOperation[ButtonIndex];
					SendEvent(Event);
				}
				ImGui::PopID();
			};

		// Keep the operation selectors within the height of their palette rows.
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, OperationPaddingY));
		// Pixel operations are available only while the I plane is enabled.
		ImGui::BeginDisabled((Flags & FCanvasOptionsFlags::Ink) == 0);
		PixelOperationLambda("Left", 0, StartPosition.y);
		PixelOperationLambda("Right", 1, RightOperationY);
		ImGui::EndDisabled();
		ImGui::PopStyleVar();
	}
}
