#include "Palette.h"
#include <Utils/UI/Draw.h>
#include <Utils/UI/Draw_ZXColorVideo.h>

namespace
{
	static const char* ThisWindowName = TEXT("Palette");
}

SPalette::SPalette(EFont::Type _FontName)
	: Super(FWindowInitializer()
	.SetName(ThisWindowName)
	.SetFontName(_FontName)
	.SetIncludeInWindows(true))
	, ColorSelectedIndex(INDEX_NONE)
{}

void SPalette::Initialize()
{}

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
	const ImVec4 ZX[] = { 
		UI::ToVec4(0x00000000), UI::ToVec4(0x0000C0FF), UI::ToVec4(0xC00000FF), UI::ToVec4(0xC000C0FF), UI::ToVec4(0x00C000FF), UI::ToVec4(0x00C0C0FF), UI::ToVec4(0xC0C000FF), UI::ToVec4(0xC0C0C0FF),
		UI::ToVec4(0x000000FF), UI::ToVec4(0x0000FFFF), UI::ToVec4(0xFF0000FF), UI::ToVec4(0xFF00FFFF), UI::ToVec4(0x00FF00FF), UI::ToVec4(0x00FFFFFF), UI::ToVec4(0xFFFF00FF), UI::ToVec4(0xFFFFFFFF) };

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.4f, 0.4f));
	for (int32_t i = 0; i < IM_ARRAYSIZE(UI::ZXSpectrumColorRGBA); ++i) {
		const bool bIsSelected = (ColorSelectedIndex == i);
		const std::string ButtonName = std::format(TEXT("IndexButton##Color {}"), i);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, bIsSelected ? 0.0f : 100.0f);

		const ImGuiColorEditFlags Flags = (!bIsSelected ? ImGuiColorEditFlags_NoBorder : ImGuiColorEditFlags_None) |
			ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop;
		if (ImGui::ColorButton(ButtonName.c_str(), UI::ToVec4(UI::ZXSpectrumColorRGBA[i]), Flags, ImVec2(24, 24)))
		{
			ColorSelectedIndex = i;
		}

		if ((i + 1) % 32 != 0 && i != 8 - 1)
		{
			ImGui::SameLine();
		}
		ImGui::PopStyleVar();
	}
	ImGui::PopStyleVar();
}
