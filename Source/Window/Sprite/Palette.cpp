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

	// Initialize independent Ink, Paper and brightness for both mouse buttons.
	for (uint8_t ButtonIndex = 0; ButtonIndex < 2; ++ButtonIndex)
	{
		ButtonSubcolor[ButtonIndex][ESubcolor::Ink] = Subcolor[ESubcolor::Ink];
		ButtonSubcolor[ButtonIndex][ESubcolor::Paper] = Subcolor[ESubcolor::Paper];
		ButtonSubcolor[ButtonIndex][ESubcolor::Bright] = Subcolor[ESubcolor::Bright];
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

					// Apply the sampled Ink, Paper and brightness only to the selected row.
					if (Event.ButtonIndex < 2)
					{
						ButtonSubcolor[Event.ButtonIndex][ESubcolor::Ink] = Subcolor[ESubcolor::Ink];
						ButtonSubcolor[Event.ButtonIndex][ESubcolor::Paper] = Subcolor[ESubcolor::Paper];
						ButtonSubcolor[Event.ButtonIndex][ESubcolor::Bright] = Subcolor[ESubcolor::Bright];
					}
				}
				else
				{
					// Store each button's Ink and Paper without replacing its other component.
					if (Event.ButtonIndex < 2)
					{
						// Keep brightness settings out of the raw mouse button color.
						if (Event.SelectedSubcolorIndex <= ESubcolor::Paper)
						{
							ButtonColor[Event.ButtonIndex] = Event.SelectedColorIndex;
						}
						// Store this row's components while Flash remains shared.
						if (Event.SelectedSubcolorIndex <= ESubcolor::Bright)
						{
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
		const float PairPreviewSize = 32.0f;
		const float ColorSize = 16.0f;
		const float ColorSpacing = 2.4f;
		// Align the shared preview with both lines of source colors.
		const float RowHeight = ImMax(PairPreviewSize, ColorSize * 2.0f + ColorSpacing);

		ImGui::PushID("SourcePalette");
		for (uint8_t ColorIndex = 0; ColorIndex < EZXColor::MAX; ++ColorIndex)
		{
			// Place the sixteen source colors beside the shared Left/Right preview.
			ImGui::SetCursorPos(StartPosition + ImVec2(ColorBox + (ColorIndex % 8) * (ColorSize + ColorSpacing),
				(ColorIndex / 8) * (ColorSize + ColorSpacing)));
			ImGui::PushID(ColorIndex);
			// Mark the selected colors for both mouse buttons.
			const bool bIsSelected = ButtonColor[0] == ColorIndex || ButtonColor[1] == ColorIndex;
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, bIsSelected ? 0.0f : 3.0f);
			uint8_t ButtonPressed = INDEX_NONE;
			if (UI::ColorButton("Color", ButtonPressed, UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorIndex]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
				ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_PressedOnClick,
				ImVec2(ColorSize, ColorSize)))
			{
				Subcolor[ESubcolor::Ink] = ColorIndex;

				if (ButtonPressed < 2)
				{
					ButtonColor[ButtonPressed] = ColorIndex;
					ButtonSubcolor[ButtonPressed][ButtonPressed] = ColorIndex;

					FEvent_Color Event;
					{
						Event.Tag = FEventTag::ChangeColorTag;
						Event.ButtonIndex = ButtonPressed;								// pressed mouse button
						Event.SelectedColorIndex = (UI::EZXSpectrumColor::Type)ColorIndex;		// zx color
						Event.SelectedSubcolorIndex = (ESubcolor::Type)ButtonPressed;	// ink (LKM), paper (RKM)
					}
					SendEvent(Event);
				}
			}

			// Keep black visible and highlight the color under the pointer.
			const bool bHovered = ImGui::IsItemHovered();
			const ImU32 BorderColor = ImGui::GetColorU32(bHovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
			const float BorderThickness = bHovered ? 2.0f : 1.0f;
			const ImVec2 BorderInset(BorderThickness * 0.5f, BorderThickness * 0.5f);
			const float BorderRounding = ImMin(ImGui::GetStyle().FrameRounding, ColorSize / 2.99f * 0.5f);
			ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() + BorderInset, ImGui::GetItemRectMax() - BorderInset,
				BorderColor, BorderRounding, ImDrawFlags_None, BorderThickness);
			// Explain which mouse button receives the selected source color.
			if (bHovered)
			{
				ImGui::SetTooltip("LMB: Left\nRMB: Right");
			}
			ImGui::PopStyleVar();
			ImGui::PopID();
		}

		// Show each mouse button's exact source color in its own half.
		ImGui::SetCursorPos(StartPosition + ImVec2(0.0f, (RowHeight - PairPreviewSize) * 0.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
		ImGui::ColorButton("LeftRight", UI::ToVec4(UI::ZXSpectrumColorRGBA[EZXColor::Transparent]),
			ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
			ImVec2(PairPreviewSize, PairPreviewSize));
		ImGui::PopStyleVar();
		const uint8_t ColorIndices[2] = { ButtonColor[0], ButtonColor[1] };
		const ImU32 Colors[2] = { ImGui::GetColorU32(UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorIndices[0]])),
			ImGui::GetColorU32(UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorIndices[1]])) };
		const ImVec2 Min = ImGui::GetItemRectMin() + ImVec2(0.75f, 0.75f);
		const ImVec2 Max = ImGui::GetItemRectMax() - ImVec2(0.75f, 0.75f);
		const float Rounding = 3.0f;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		// Fill the upper-left Left half along its rounded outer corners.
		DrawList->PathArcTo(ImVec2(Min.x + Rounding, Max.y - Rounding), Rounding, IM_PI * 0.75f, IM_PI);
		DrawList->PathArcTo(Min + ImVec2(Rounding, Rounding), Rounding, IM_PI, IM_PI * 1.5f);
		DrawList->PathArcTo(ImVec2(Max.x - Rounding, Min.y + Rounding), Rounding, IM_PI * 1.5f, IM_PI * 1.75f);
		DrawList->PathFillConvex(Colors[0]);
		// Fill the lower-right Right half along its rounded outer corners.
		DrawList->PathArcTo(ImVec2(Max.x - Rounding, Min.y + Rounding), Rounding, IM_PI * 1.75f, IM_PI * 2.0f);
		DrawList->PathArcTo(Max - ImVec2(Rounding, Rounding), Rounding, 0.0f, IM_PI * 0.5f);
		DrawList->PathArcTo(ImVec2(Min.x + Rounding, Max.y - Rounding), Rounding, IM_PI * 0.5f, IM_PI * 0.75f);
		DrawList->PathFillConvex(Colors[1]);
		// End the diagonal at the rounded outline.
		const float DiagonalInset = Rounding * (1.0f - 0.70710678f);
		DrawList->AddLine(ImVec2(Min.x + DiagonalInset, Max.y - DiagonalInset),
			ImVec2(Max.x - DiagonalInset, Min.y + DiagonalInset), ImGui::GetColorU32(ImGuiCol_TextDisabled));
		DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImGuiCol_TextDisabled), Rounding);

		for (uint8_t Component = 0; Component < 2; ++Component)
		{
			const char* Label = Component == 0 ? "L" : "R";
			// Center each label in its own triangle and keep it readable over the fill.
			const ImVec2 LabelPosition = Min + (Max - Min) * (Component == 0 ? 0.25f : 0.75f) - ImGui::CalcTextSize(Label) * 0.5f;
			const ImVec4 Fill = UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorIndices[Component]]);
			const bool bLightFill = (Fill.x + Fill.y * 2.0f + Fill.z) > 1.6f;
			const ImU32 LabelColor = ImGui::GetColorU32(bLightFill ? ImVec4(0, 0, 0, 1) : ImVec4(1, 1, 1, 1));
			const ImU32 ShadowColor = ImGui::GetColorU32(bLightFill ? ImVec4(1, 1, 1, 1) : ImVec4(0, 0, 0, 1));
			DrawList->AddText(LabelPosition + ImVec2(1.0f, 1.0f), ShadowColor, Label);
			DrawList->AddText(LabelPosition, LabelColor, Label);
		}

		ImGui::PopID();
	}
	else
	{
		const uint8_t Flags = OptionsFlags & ~FCanvasOptionsFlags::Source;
		const float PairPreviewSize = 32.0f;
		const float ColorSize = 16.0f;
		const float ColorSpacing = 2.4f;
		// Fit two palette lines into each mouse button row.
		const float RowHeight = ImMax(PairPreviewSize, ColorSize * 2.0f + ColorSpacing);
		const float OperationX = StartPosition.x + PaletteBox + (ColorSize + ColorSpacing) * 8.0f + 16.0f;
		const float OperationPaddingY = ImMax(0.0f, (PreviewSize - ImGui::GetFontSize()) * 0.5f);

		auto RowColorLambda = [this](uint8_t ButtonIndex, ESubcolor::Type Component)
			{
				uint8_t ColorIndex = ButtonSubcolor[ButtonIndex][Component];
				// Keep transparency distinct from opaque black.
				if (ColorIndex != EZXColor::Transparent)
				{
					// Apply this row's common brightness to either component.
					ColorIndex = (ColorIndex & 0x07) | ((ButtonSubcolor[ButtonIndex][ESubcolor::Bright] == EZXColor::True) << 3);
					ColorIndex = ColorIndex == EZXColor::Black ? EZXColor::Black_ : ColorIndex;
				}
				return ColorIndex;
			};

		ImGui::PushID("IPMPalette");
		for (uint8_t ButtonIndex = 0; ButtonIndex < 2; ++ButtonIndex)
		{
			ImGui::PushID(ButtonIndex);
			// Position one complete palette next to its drawing-button label.
			const ImVec2 RowPosition = StartPosition + ImVec2(0.0f, ButtonIndex * (RowHeight + 12.0f));
			ImGui::SetCursorPos(RowPosition + ImVec2(0.0f, (RowHeight - ImGui::GetFontSize()) * 0.5f));
			ImGui::TextUnformatted(ButtonIndex == 0 ? "Left" : "Right");

			for (uint8_t ColorIndex = 0; ColorIndex < EZXColor::MAX; ++ColorIndex)
			{
				// Arrange indices 0-15 in two lines, with transparency at index zero.
				ImGui::SetCursorPos(RowPosition + ImVec2(PaletteBox + (ColorIndex % 8) * (ColorSize + ColorSpacing),
					(ColorIndex / 8) * (ColorSize + ColorSpacing)));
				ImGui::PushID(ColorIndex);
				// Mark both selected components within this row's palette.
				const bool bIsSelected = ColorIndex == RowColorLambda(ButtonIndex, ESubcolor::Ink) ||
					ColorIndex == RowColorLambda(ButtonIndex, ESubcolor::Paper);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, bIsSelected ? 0.0f : 3.0f);
				uint8_t ButtonPressed = INDEX_NONE;
				// The row selects the drawing button, while the click selects Ink or Paper.
				if (UI::ColorButton("Color", ButtonPressed, UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorIndex]),
					ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
					ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_PressedOnClick,
					ImVec2(ColorSize, ColorSize)) && ButtonPressed < 2)
				{
					const ESubcolor::Type Component = ButtonPressed == 0 ? ESubcolor::Ink : ESubcolor::Paper;
					// Apply Alt-click to both drawing buttons, otherwise only to this row.
					const bool bBothButtons = ImGui::GetIO().KeyAlt;
					const uint8_t FirstButtonIndex = bBothButtons ? 0 : ButtonIndex;
					const uint8_t EndButtonIndex = bBothButtons ? 2 : ButtonIndex + 1;
					for (uint8_t TargetButtonIndex = FirstButtonIndex; TargetButtonIndex < EndButtonIndex; ++TargetButtonIndex)
					{
						// Update the clicked component for this drawing button.
						ButtonColor[TargetButtonIndex] = ColorIndex;
						ButtonSubcolor[TargetButtonIndex][Component] = ColorIndex;
						Subcolor[Component] = ColorIndex;
						FEvent_Color Event;
						Event.Tag = FEventTag::ChangeColorTag;
						Event.ButtonIndex = TargetButtonIndex;
						Event.SelectedColorIndex = (EZXColor)ColorIndex;
						Event.SelectedSubcolorIndex = Component;
						SendEvent(Event);

						// Black and transparency have no separate bright shade.
						if ((ColorIndex & 0x07) != 0)
						{
							const EZXColor Bright = (ColorIndex & 0x08) ? EZXColor::True : EZXColor::False;
							// Apply the selected shade to both Ink and Paper of this row.
							ButtonSubcolor[TargetButtonIndex][ESubcolor::Bright] = Bright;
							Subcolor[ESubcolor::Bright] = Bright;
							Event.SelectedColorIndex = Bright;
							Event.SelectedSubcolorIndex = ESubcolor::Bright;
							SendEvent(Event);
						}
					}
				}

				// Keep black visible and highlight the color under the pointer.
				const bool bHovered = ImGui::IsItemHovered();
				const ImU32 BorderColor = ImGui::GetColorU32(bHovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
				const float BorderThickness = bHovered ? 2.0f : 1.0f;
				const ImVec2 BorderInset(BorderThickness * 0.5f, BorderThickness * 0.5f);
				const float BorderRounding = ImMin(ImGui::GetStyle().FrameRounding, ColorSize / 2.99f * 0.5f);
				ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin() + BorderInset, ImGui::GetItemRectMax() - BorderInset,
					BorderColor, BorderRounding, ImDrawFlags_None, BorderThickness);
				// Explain which component each click changes within the current row.
				if (bHovered)
				{
					ImGui::SetTooltip("LMB: Ink\nRMB: Paper\nAlt: apply to Left and Right");
				}
				ImGui::PopStyleVar();
				ImGui::PopID();
			}

			// Draw a larger rounded Ink/Paper preview using this row's brightness.
			ImGui::SetCursorPos(RowPosition + ImVec2(ColorBox, (RowHeight - PairPreviewSize) * 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
			ImGui::ColorButton("InkPaper", UI::ToVec4(UI::ZXSpectrumColorRGBA[EZXColor::Transparent]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
				ImVec2(PairPreviewSize, PairPreviewSize));
			ImGui::PopStyleVar();
			const uint8_t ColorIndices[2] = { RowColorLambda(ButtonIndex, ESubcolor::Ink), RowColorLambda(ButtonIndex, ESubcolor::Paper) };
			const ImU32 Colors[2] = { ImGui::GetColorU32(UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorIndices[0]])),
				ImGui::GetColorU32(UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorIndices[1]])) };
			const ImVec2 Min = ImGui::GetItemRectMin() + ImVec2(0.75f, 0.75f);
			const ImVec2 Max = ImGui::GetItemRectMax() - ImVec2(0.75f, 0.75f);
			const float Rounding = 3.0f;
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			// Fill the upper-left Ink half along its rounded outer corners.
			DrawList->PathArcTo(ImVec2(Min.x + Rounding, Max.y - Rounding), Rounding, IM_PI * 0.75f, IM_PI);
			DrawList->PathArcTo(Min + ImVec2(Rounding, Rounding), Rounding, IM_PI, IM_PI * 1.5f);
			DrawList->PathArcTo(ImVec2(Max.x - Rounding, Min.y + Rounding), Rounding, IM_PI * 1.5f, IM_PI * 1.75f);
			DrawList->PathFillConvex(Colors[0]);
			// Fill the lower-right Paper half along its rounded outer corners.
			DrawList->PathArcTo(ImVec2(Max.x - Rounding, Min.y + Rounding), Rounding, IM_PI * 1.75f, IM_PI * 2.0f);
			DrawList->PathArcTo(Max - ImVec2(Rounding, Rounding), Rounding, 0.0f, IM_PI * 0.5f);
			DrawList->PathArcTo(ImVec2(Min.x + Rounding, Max.y - Rounding), Rounding, IM_PI * 0.5f, IM_PI * 0.75f);
			DrawList->PathFillConvex(Colors[1]);
			// End the diagonal at the rounded outline.
			const float DiagonalInset = Rounding * (1.0f - 0.70710678f);
			DrawList->AddLine(ImVec2(Min.x + DiagonalInset, Max.y - DiagonalInset),
				ImVec2(Max.x - DiagonalInset, Min.y + DiagonalInset), ImGui::GetColorU32(ImGuiCol_TextDisabled));
			DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImGuiCol_TextDisabled), Rounding);

			for (uint8_t Component = 0; Component < 2; ++Component)
			{
				const char* Label = Component == 0 ? "I" : "P";
				// Center each label in its own triangle and keep it readable over the fill.
				const ImVec2 LabelPosition = Min + (Max - Min) * (Component == 0 ? 0.25f : 0.75f) - ImGui::CalcTextSize(Label) * 0.5f;
				const ImVec4 Fill = UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorIndices[Component]]);
				const bool bLightFill = (Fill.x + Fill.y * 2.0f + Fill.z) > 1.6f;
				const ImU32 LabelColor = ImGui::GetColorU32(bLightFill ? ImVec4(0, 0, 0, 1) : ImVec4(1, 1, 1, 1));
				const ImU32 ShadowColor = ImGui::GetColorU32(bLightFill ? ImVec4(1, 1, 1, 1) : ImVec4(0, 0, 0, 1));
				DrawList->AddText(LabelPosition + ImVec2(1.0f, 1.0f), ShadowColor, Label);
				DrawList->AddText(LabelPosition, LabelColor, Label);
			}

			static const char* OperationNames[] = { "SET", "RES", "XOR", "None" };
			int32_t OperationIndex = ButtonPixelOperation[ButtonIndex];
			ImGui::SetCursorPos(ImVec2(OperationX, RowPosition.y));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, OperationPaddingY));
			ImGui::SetNextItemWidth(70.0f);
			// Pixel operations are available only while the I plane is enabled.
			ImGui::BeginDisabled((Flags & FCanvasOptionsFlags::Ink) == 0);
			if (ImGui::Combo("##PixelOperation", &OperationIndex, OperationNames, IM_ARRAYSIZE(OperationNames)))
			{
				ButtonPixelOperation[ButtonIndex] = (EPixelOperation::Type)OperationIndex;
				// Publish the operation for this drawing-button row.
				FEvent_Color Event;
				Event.Tag = FEventTag::ChangePixelOperationTag;
				Event.ButtonIndex = ButtonIndex;
				Event.PixelOperation = ButtonPixelOperation[ButtonIndex];
				SendEvent(Event);
			}
			ImGui::EndDisabled();
			ImGui::PopStyleVar();
			ImGui::PopID();
		}
		ImGui::PopID();
	}
}
