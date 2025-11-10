#include "Draw.h"
#include "Fonts.h"

void UI::DrawTable(const char* TableID, ImGuiTableFlags Flags, bool bEnabled, const std::vector<FColumn>& Columns)
{
	if (ImGui::BeginTable(TableID, (int32_t)Columns.size(), Flags))
	{
		for (const FColumn& Column : Columns)
		{
			ImGui::TableSetupColumn(Column.Name, Column.Flags, Column.InitWidth, Column.UserID);
		}
		ImGui::TableHeadersRow();

		if (!bEnabled) ImGui::BeginDisabled();
		ImGui::TableNextRow();
		for (const FColumn& Column : Columns)
		{
			ImGui::TableNextColumn();
			Column.DrawFunction();
		}
		if (!bEnabled) ImGui::EndDisabled();

		ImGui::EndTable();
	}
}

void UI::DrawTooltip(const char* Text)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::BeginItemTooltip())
	{
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(Text);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void UI::DrawProperty(const char* PropertyName, const char* Value, const char* Tooltip /*= nullptr*/, const ImVec4& Color /*= COLOR_WHITE*/)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	ImGui::PushStyleColor(ImGuiCol_Text, ToVec4(0x909090FF));
	TextAligned(PropertyName, { 1.0f, 0.5f });
	ImGui::PopStyleColor();

	ImGui::TableNextColumn();
	ImGui::Dummy(ImVec2(5.0f, 0.0f)); ImGui::SameLine();
	ImGui::PushStyleColor(ImGuiCol_Text, Color);
	ImGui::Text(Value);
	ImGui::PopStyleColor();

	if (Tooltip) {
		ImGui::SameLine();
		DrawTooltip(Tooltip);
	}
}

int32_t UI::GetVisibleLines(EFont::Type FontName, float CutHeight /*= 0.0f*/)
{
	ImFont* FoundFont = FFonts::Get().GetFont(FontName);
	if (FoundFont == nullptr || FoundFont->Sources.empty())
	{
		return INDEX_NONE;
	}
	const ImVec2 AvailableSpace = ImGui::GetContentRegionAvail();
	const float Number = ((AvailableSpace.y - CutHeight) / FoundFont->Sources.front()->SizePixels) * (1.0f / FoundFont->Scale);
	return int32_t(Number) + (std::fmod(Number, 1.0f) == 0.0f ? -1 : 0);
}

void UI::TextAligned(const char* Text, const ImVec2& Aligment, const ImVec2* _Padding /*= nullptr*/)
{
	const ImVec2 SizeArg = ImVec2(-FLT_MIN, 0.0f);

	ImGuiWindow* Window = ImGui::GetCurrentWindow();
	if (Window->SkipItems)
	{
		return;
	}

	ImVec2 Padding;
	if (_Padding == nullptr)
	{
		ImGuiContext& Context = *GImGui;
		ImGuiStyle& Style = Context.Style;
		Padding = Style.FramePadding;
	}
	else
	{
		Padding = *_Padding;
	}

	const ImGuiID GuiID = Window->GetID(Text);
	const ImVec2 LabelSize = ImGui::CalcTextSize(Text, NULL, true);

	const ImVec2 Position = Window->DC.CursorPos;
	const ImVec2 Size = ImGui::CalcItemSize(SizeArg, LabelSize.x + Padding.x * 2.0f, LabelSize.y + Padding.y * 2.0f);

	const ImRect bb(Position, Position + Size);
	ImGui::ItemSize(Size, Padding.y);

	if (!ImGui::ItemAdd(bb, GuiID))
	{
		return;
	}

	ImGui::RenderTextClipped(bb.Min + Padding, bb.Max - Padding, Text, NULL, &LabelSize, Aligment, &bb);
}

bool UI::Button(const char* Label, bool bIsPressed, const ImVec2& SizeArg, bool bEnabled, const ImVec2& Aligment)
{
	ON_SCOPE_EXIT
	{
		if (!bEnabled) ImGui::EndDisabled();
	};

	if (!bEnabled) ImGui::BeginDisabled();

	ImGuiWindow* Window = ImGui::GetCurrentWindow();
	if (Window->SkipItems)
	{
		return false;
	}

	ImGuiContext& Context = *GImGui;
	const ImGuiStyle& Style = Context.Style;
	const ImGuiID ID = Window->GetID(Label);
	const ImVec2 LabelSize = ImGui::CalcTextSize(Label, NULL, true);

	ImVec2 Pos = Window->DC.CursorPos;
	ImVec2 Size = ImGui::CalcItemSize(SizeArg, LabelSize.x + Style.FramePadding.x * 2.0f, LabelSize.y + Style.FramePadding.y * 2.0f);

	const ImRect bb(Pos, Pos + Size);
	ImGui::ItemSize(Size, Style.FramePadding.y);
	if (!ImGui::ItemAdd(bb, ID))
	{
		return false;
	}

	bool bHovered, bHeld;
	bool pressed = ImGui::ButtonBehavior(bb, ID, &bHovered, &bHeld);

	// Render
	const ImU32 ButtonColor = ImGui::GetColorU32(bIsPressed ? ImGuiCol_ButtonActive : (bHeld && bHovered) ? ImGuiCol_ButtonActive : bHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
	ImGui::RenderNavHighlight(bb, ID);
	ImGui::RenderFrame(bb.Min, bb.Max, ButtonColor, true, bIsPressed ? 5.0f : 0.0f);
	ImGui::RenderTextClipped(bb.Min + Style.FramePadding, bb.Max - Style.FramePadding, Label, NULL, &LabelSize, Aligment, &bb);

	return pressed;
}

bool UI::ColorButton(const char* LabelID, uint8_t& OutputPressedButton, const ImVec4& Color, ImGuiColorEditFlags ColorFlags, ImGuiButtonFlags ButtonFlags, const ImVec2& SizeArg)
{
	ImGuiWindow* window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiContext& g = *GImGui;
	const ImGuiID id = window->GetID(LabelID);
	const float default_size = ImGui::GetFrameHeight();
	const ImVec2 size(SizeArg.x == 0.0f ? default_size : SizeArg.x, SizeArg.y == 0.0f ? default_size : SizeArg.y);
	const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size);
	ImGui::ItemSize(bb, (size.y >= default_size) ? g.Style.FramePadding.y : 0.0f);
	if (!ImGui::ItemAdd(bb, id))
	{
		return false;
	}

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ButtonFlags);
	OutputPressedButton = g.ActiveIdMouseButton;

	if (ColorFlags & ImGuiColorEditFlags_NoAlpha)
		ColorFlags &= ~(ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf);

	ImVec4 col_rgb = Color;
	if (ColorFlags & ImGuiColorEditFlags_InputHSV)
		ImGui::ColorConvertHSVtoRGB(col_rgb.x, col_rgb.y, col_rgb.z, col_rgb.x, col_rgb.y, col_rgb.z);

	ImVec4 col_rgb_without_alpha(col_rgb.x, col_rgb.y, col_rgb.z, 1.0f);
	float grid_step = ImMin(size.x, size.y) / 2.99f;
	float rounding = ImMin(g.Style.FrameRounding, grid_step * 0.5f);
	ImRect bb_inner = bb;
	float off = 0.0f;
	if ((ColorFlags & ImGuiColorEditFlags_NoBorder) == 0)
	{
		off = -0.75f; // The border (using Col_FrameBg) tends to look off when color is near-opaque and rounding is enabled. This offset seemed like a good middle ground to reduce those artifacts.
		bb_inner.Expand(off);
	}
	if ((ColorFlags & ImGuiColorEditFlags_AlphaPreviewHalf) && col_rgb.w < 1.0f)
	{
		float mid_x = IM_ROUND((bb_inner.Min.x + bb_inner.Max.x) * 0.5f);
		ImGui::RenderColorRectWithAlphaCheckerboard(window->DrawList, ImVec2(bb_inner.Min.x + grid_step, bb_inner.Min.y), bb_inner.Max, ImGui::GetColorU32(col_rgb), grid_step, ImVec2(-grid_step + off, off), rounding, ImDrawFlags_RoundCornersRight);
		window->DrawList->AddRectFilled(bb_inner.Min, ImVec2(mid_x, bb_inner.Max.y), ImGui::GetColorU32(col_rgb_without_alpha), rounding, ImDrawFlags_RoundCornersLeft);
	}
	else
	{
		// Because GetColorU32() multiplies by the global style Alpha and we don't want to display a checkerboard if the source code had no alpha
		ImVec4 col_source = (ColorFlags & ImGuiColorEditFlags_AlphaPreview) ? col_rgb : col_rgb_without_alpha;
		if (col_source.w < 1.0f)
			ImGui::RenderColorRectWithAlphaCheckerboard(window->DrawList, bb_inner.Min, bb_inner.Max, ImGui::GetColorU32(col_source), grid_step, ImVec2(off, off), rounding);
		else
			window->DrawList->AddRectFilled(bb_inner.Min, bb_inner.Max, ImGui::GetColorU32(col_source), rounding);
	}
	ImGui::RenderNavHighlight(bb, id);
	if ((ColorFlags & ImGuiColorEditFlags_NoBorder) == 0)
	{
		if (g.Style.FrameBorderSize > 0.0f)
			ImGui::RenderFrameBorder(bb.Min, bb.Max, rounding);
		else
			window->DrawList->AddRect(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), rounding); // Color button are often in need of some sort of border
	}

	// Drag and Drop Source
	// NB: The ActiveId test is merely an optional micro-optimization, BeginDragDropSource() does the same test.
	if (g.ActiveId == id && !(ColorFlags & ImGuiColorEditFlags_NoDragDrop) && ImGui::BeginDragDropSource())
	{
		if (ColorFlags & ImGuiColorEditFlags_NoAlpha)
			ImGui::SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F, &col_rgb, sizeof(float) * 3, ImGuiCond_Once);
		else
			ImGui::SetDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F, &col_rgb, sizeof(float) * 4, ImGuiCond_Once);
		ImGui::ColorButton(LabelID, Color, ColorFlags);
		ImGui::SameLine();
		ImGui::TextEx("Color");
		ImGui::EndDragDropSource();
	}

	// Tooltip
	if (!(ColorFlags & ImGuiColorEditFlags_NoTooltip) && hovered && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
		ImGui::ColorTooltip(LabelID, &Color.x, ColorFlags & (ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaPreview | ImGuiColorEditFlags_AlphaPreviewHalf));

	return pressed;
}
