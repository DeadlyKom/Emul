#include "Palette.h"
#include <Utils/UI/Draw.h>
#include <Utils/UI/Draw_ZXColorVideo.h>
#include "Events.h"
#include "Canvas.h"

namespace
{
	static const char* ThisWindowName = TEXT("Palette");
}

SPalette::SPalette(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
{
	ButtonColor[0] = UI::EZXSpectrumColor::Black_;	// left button
	ButtonColor[1] = UI::EZXSpectrumColor::White_;	// right button

	Subcolor[ESubcolor::Ink] = UI::EZXSpectrumColor::Black_;
	Subcolor[ESubcolor::Paper] = UI::EZXSpectrumColor::Black;
	Subcolor[ESubcolor::Bright] = UI::EZXSpectrumColor::False;
	Subcolor[ESubcolor::Flash] = UI::EZXSpectrumColor::False;
}

void SPalette::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_Canvas>(
		[this](const FEvent_Canvas& Event)
		{
			if (Event.Tag == FEvent_Canvas::CanvasOptionsFlagsTag)
			{
				OptionsFlags = Event.OptionsFlags;
			}
			else if (Event.Tag == FEvent_Canvas::SelectedColorTag)
			{
				ButtonColor[Event.SelectedColor.ButtonIndex & 0x01] = Event.SelectedColor.SelectedColorIndex;
				if (Event.SelectedColor.SelectedSubcolorIndex < ESubcolor::MAX)
				{
					Subcolor[Event.SelectedColor.SelectedSubcolorIndex] = Event.SelectedColor.SelectedColorIndex;
				}
			}
		});
}

void SPalette::Initialize()
{
}

void SPalette::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	{
		Display_Colors();
		ImGui::End();
	}
}

void SPalette::Display_Colors()
{
	const float ColorBox = 50.0f;
	const float PaletteBox = 100.0f;
	const ImVec2 StartPosition = ImGui::GetCursorPos();
	if (OptionsFlags & FCanvasOptionsFlags::Source)
	{
		ImGui::Text("Left");
		ImGui::SameLine();
		ImGui::SetCursorPosX(ColorBox);
		ImGui::ColorButton(TEXT("LeftButton##Color"),
			UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[0]]),
			ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(16, 16));

		ImGui::Text("Right");
		ImGui::SameLine();
		ImGui::SetCursorPosX(ColorBox);
		ImGui::ColorButton(TEXT("RightButton##Color"),
			UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[1]]),
			ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(16, 16));

		ImGui::SetCursorPos(StartPosition + ImVec2(PaletteBox, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.4f, 4.4f));
		for (int32_t i = 0; i < IM_ARRAYSIZE(UI::ZXSpectrumColorRGBA); ++i) {
			const bool bIsSelected = (Subcolor[ESubcolor::Ink] == i);
			const std::string ButtonName = std::format(TEXT("IndexButton##Color {}"), i);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, bIsSelected ? 0.0f : 100.0f);

			const ImGuiColorEditFlags Flags = (!bIsSelected ? ImGuiColorEditFlags_NoBorder : ImGuiColorEditFlags_None) |
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop;
			uint8_t ButtonPressed = -1;
			if (UI::ColorButton(ButtonName.c_str(), ButtonPressed, UI::ToVec4(UI::ZXSpectrumColorRGBA[i]), Flags,
				ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_PressedOnClick, ImVec2(16, 16)))
			{
				Subcolor[ESubcolor::Ink] = i;

				if (ButtonPressed < 2)
				{
					ButtonColor[ButtonPressed] = i;

					FEvent_PaletteBar Event;
					Event.Tag = FEvent_PaletteBar::ColorIndexTag;
					FSelectedColor SelectedColor
					{
						.ButtonIndex = ButtonPressed,
						.SelectedColorIndex = (UI::EZXSpectrumColor::Type)i,
						.SelectedSubcolorIndex = (ESubcolor::Type)ButtonPressed,
					};
					Event.SelectedColor = SelectedColor;
					SendEvent(Event);
				}
			}

			if ((i + 1) % 32 != 0 && i != 8 - 1)
			{
				ImGui::SameLine();
			}
			else
			{
				ImGui::SetCursorPosX(StartPosition.x + PaletteBox);
			}
			ImGui::PopStyleVar();
		}
		ImGui::PopStyleVar();
	}
	else
	{
		static constexpr uint8_t IPColor[] = {
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
		static constexpr uint8_t IColor[] = {
			UI::EZXSpectrumColor::Black_,
			UI::EZXSpectrumColor::White,
		};
		static constexpr uint8_t MColor[] = {
			UI::EZXSpectrumColor::Black,
			UI::EZXSpectrumColor::Black_,
		};
		static constexpr uint8_t IMColor[] = {
			UI::EZXSpectrumColor::Black,
			UI::EZXSpectrumColor::Black_,
			UI::EZXSpectrumColor::White,
		};
		static constexpr uint8_t BrightColor[] = {
			UI::EZXSpectrumColor::White,
			UI::EZXSpectrumColor::White_,
		};
		static constexpr uint8_t BrightSelect[] = {
			UI::EZXSpectrumColor::False,
			UI::EZXSpectrumColor::True,
		};

		const uint8_t* InkColorArray = IPColor;
		const uint8_t* PaperColorArray = IPColor;
		const uint8_t* BrightColorArray = BrightColor;

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

		uint8_t Ink = Subcolor[ESubcolor::Ink] == UI::EZXSpectrumColor::Transparent ? UI::EZXSpectrumColor::Transparent : Subcolor[ESubcolor::Ink] | (bBright << 3);
		uint8_t Paper = Subcolor[ESubcolor::Paper] == UI::EZXSpectrumColor::Transparent ? UI::EZXSpectrumColor::Transparent : Subcolor[ESubcolor::Paper] | (bBright << 3);
		const uint8_t Bright = BrightColor[bBright];

		if (!bLRButton)
		{
			ImGui::Text("Ink");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			ImGui::ColorButton(TEXT("InkButton##Color"),
				UI::ToVec4(UI::ZXSpectrumColorRGBA[Ink]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(16, 16));
		}
		else
		{
			ImGui::Text("Left");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			ImGui::ColorButton(TEXT("LeftButton##Color"),
				UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[0]]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(16, 16));
		}

		if (PaperColorArray)
		{
			ImGui::Text("Paper");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			ImGui::ColorButton(TEXT("PaperButton##Color"),
				UI::ToVec4(UI::ZXSpectrumColorRGBA[Paper]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(16, 16));
		}
		else if (bLRButton)
		{
			ImGui::Text("Right");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			ImGui::ColorButton(TEXT("RightButton##Color"),
				UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[1]]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(16, 16));
		}
		if (BrightColorArray)
		{
			ImGui::Text("Bright");
			ImGui::SameLine();
			ImGui::SetCursorPosX(ColorBox);
			ImGui::ColorButton(TEXT("BrightButton##Color"),
				UI::ToVec4(UI::ZXSpectrumColorRGBA[Bright]),
				ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(16, 16));
		}

		auto ColorButtonsLambda = [=, this](ESubcolor::Type SubColorIndex, const uint8_t* ColorArray, int32_t Size, const uint8_t* SelectArray)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.4f, 4.4f));
				for (int32_t i = 0; i < Size; ++i) {
					const bool bIsSelected = (Subcolor[SubColorIndex] == i);
					const std::string ButtonName = std::format(TEXT("IndexButton##Color {}"), i);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, bIsSelected ? 0.0f : 100.0f);

					const ImGuiColorEditFlags Flags = (!bIsSelected ? ImGuiColorEditFlags_NoBorder : ImGuiColorEditFlags_None) |
						ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop;
					uint8_t ButtonPressed = -1;
					const uint8_t SelectValue = SelectArray != nullptr ? SelectArray[i] : ColorArray[i];
					if (UI::ColorButton(ButtonName.c_str(), ButtonPressed, UI::ToVec4(UI::ZXSpectrumColorRGBA[ColorArray[i]]), Flags,
						ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_PressedOnClick, ImVec2(16, 16)))
					{
						
						Subcolor[SubColorIndex] = SelectValue;

						if (ButtonPressed < 2)
						{
							ButtonColor[ButtonPressed] = Subcolor[SubColorIndex];

							FEvent_PaletteBar Event;
							Event.Tag = FEvent_PaletteBar::ColorIndexTag;
							const uint8_t IPColor = SubColorIndex < 2 ? SelectValue | (bBright << 3) : SelectValue;
							FSelectedColor SelectedColor
							{
								.ButtonIndex = ButtonPressed,
								.SelectedColorIndex = (UI::EZXSpectrumColor::Type)IPColor,
								.SelectedSubcolorIndex = SubColorIndex,
							};
							Event.SelectedColor = SelectedColor;
							SendEvent(Event);
						}
					}

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
			ImGui::SetCursorPosX(StartPosition.x + PaletteBox);
			ColorButtonsLambda(ESubcolor::Paper, PaperColorArray, PaperColorSize, nullptr);
		}
		if (BrightColorArray)
		{
			ImGui::SetCursorPosX(StartPosition.x + PaletteBox);
			ColorButtonsLambda(ESubcolor::Bright, BrightColorArray, BrightColorSize, BrightSelect);
		}
	}
}
