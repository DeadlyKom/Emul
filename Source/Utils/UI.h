#pragma once

#include <CoreMinimal.h>

namespace EFont { enum Type; }

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
	//  Vec4(r,g,b,a) to ImU32(a,b,g,r)
	static constexpr ImU32 ColorToU32(const ImVec4& Color)
	{
		return 
			int(Color.x * 255.0f) << IM_COL32_R_SHIFT |
			int(Color.y * 255.0f) << IM_COL32_G_SHIFT |
			int(Color.z * 255.0f) << IM_COL32_B_SHIFT |
			int(Color.w * 255.0f) << IM_COL32_A_SHIFT;
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

	// Define a message as an enumeration.
#define REGISTER_COLOR(num,name,color) name = num,
namespace EColor
{
	enum Type : int32_t
	{
		// Include all the hard-coded names
		#include "UI_DefaultColor.inl"

		// Special constant for the last hard-coded name index
		MaxHardcodedIndex,
	};
}
#undef REGISTER_COLOR

#define REGISTER_COLOR(num,name,color) { color },
	static constexpr ImVec4 Colors[EColor::MaxHardcodedIndex] =
	{
		#include "UI_DefaultColor.inl"
	};
#undef REGISTER_COLOR

#define REGISTER_COLOR(num,name,color) inline constexpr EColor::Type COLOR_##name = EColor::name;
#include "UI_DefaultColor.inl"
#undef REGISTER_COLOR

#define COL_REF(color)		(&UI::Colors[color])
#define COL_CONST(color)	(UI::Colors[color])
#define COL_CONST32(color)	(UI::ColorToU32(UI::Colors[color]))

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
	void DrawProperty(const char* PropertyName, const char* Value,const char* Tooltip = nullptr, const ImVec4& Color = Colors[COLOR_WHITE]);

	int32_t GetVisibleLines(EFont::Type FontName);
	void TextAligned(const char* Text, const ImVec2& Aligment = { 1.0f, 0.5f });
}