#include "UI.h"
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

int32_t UI::GetVisibleLines(EFont::Type FontName)
{
	ImFont* FoundFont = FFonts::Get().GetFont(FontName);
	if (FoundFont == nullptr)
	{
		return INDEX_NONE;
	}

	return int32_t(((ImGui::GetWindowSize().y - ImGui::GetCursorPosY()) / FoundFont->ConfigData->SizePixels) * (1.0 / FoundFont->Scale));
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
