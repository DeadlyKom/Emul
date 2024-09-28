#pragma once

#include <CoreMinimal.h>

namespace UI
{
#define COLOR_32_R_MASK     0xFF000000
#define COLOR_32_G_MASK     0x00FF0000
#define COLOR_32_B_MASK     0x0000FF00
#define COLOR_32_A_MASK     0x000000FF

#define COLOR_32_R_SHIFT    24
#define COLOR_32_G_SHIFT    16
#define COLOR_32_B_SHIFT    8
#define COLOR_32_A_SHIFT    0

	// 0xRRGGBBAA to Vec4(r,g,b,a)
	static constexpr ImVec4 ToVec4(const uint32_t Color)
	{
		return ImVec4(
			((Color & COLOR_32_R_MASK) >> COLOR_32_R_SHIFT) / 255.0f,
			((Color & COLOR_32_G_MASK) >> COLOR_32_G_SHIFT) / 255.0f,
			((Color & COLOR_32_B_MASK) >> COLOR_32_B_SHIFT) / 255.0f,
			((Color & COLOR_32_A_MASK) >> COLOR_32_A_SHIFT) / 255.0f);
	}
	// 0xRGBA to 0xABGR
	static constexpr ImU32 ToU32(const uint32_t Color)
	{
		return
			(Color & COLOR_32_R_MASK) >> COLOR_32_R_SHIFT << IM_COL32_R_SHIFT |
			(Color & COLOR_32_G_MASK) >> COLOR_32_G_SHIFT << IM_COL32_G_SHIFT |
			(Color & COLOR_32_B_MASK) >> COLOR_32_B_SHIFT << IM_COL32_B_SHIFT |
			(Color & COLOR_32_A_MASK) >> COLOR_32_A_SHIFT << IM_COL32_A_SHIFT;
	}

	// disasm text colors
	static constexpr ImVec4 COLOR_WHITE = ToVec4(0xFFFFFFFF);
	static constexpr ImVec4 COLOR_NUMBER = ToVec4(0xD0D0D0FF);
	static constexpr ImVec4 COLOR_NUMBER_UPDATED = ToVec4(0xD0902FFF);

	struct FColumn
	{
		const char* Name;
		ImGuiTableColumnFlags Flags;
		float InitWidth;
		ImGuiID UserID;
		std::function<void()> DrawFunction;
	};

	void DrawTable(const char* TableID, ImGuiTableFlags Flags, bool bEnabled, const std::vector<UI::FColumn>& Columns);

	void DrawTooltip(const char* Text);
	void DrawProperty(const char* PropertyName, const char* Value,const char* Tooltip = nullptr, const ImVec4& Color = COLOR_WHITE);

	void TextAligned(const char* Text, const ImVec2& Aligment = { 1.0f, 0.5f });
}