#include "Draw_Oscillogram.h"
#include "Utils/UI/Draw.h"

void UI::Draw_Oscillogram(const char* TableID, FOscillograph& Oscillograph, const ImVec2& SizeArg /*= ImVec2(-1.0f, -1.0f)*/, const ImVec2* _Padding /*= nullptr*/)
{
	ImGuiWindow* Window = ImGui::GetCurrentWindow();
	if (Window->SkipItems)
	{
		return;
	}

	ImGuiContext& Context = *GImGui;
	ImGuiStyle& Style = Context.Style;
	ImDrawList& DrawList = *Window->DrawList;

	ImVec2 Padding;
	if (_Padding == nullptr)
	{
		Padding = Style.FramePadding;
	}
	else
	{
		Padding = *_Padding;
	}

	// calculate frame bb
	const ImGuiID GuiID = Window->GetID(TableID);
	const ImVec2 Position = Window->DC.CursorPos;
	const ImVec2 Size = ImGui::CalcItemSize(SizeArg, Padding.x * 2.0f, Padding.y * 2.0f);

	const ImRect FrameRect(Position, Position + Size);
	ImGui::ItemSize(Size, Padding.y);
	if (!ImGui::ItemAdd(FrameRect, GuiID))
	{
		return;
	}

	// calculate canvas bb
	const ImRect CanvasRect(FrameRect.Min + Oscillograph.Style.OscillograPadding, FrameRect.Max - Oscillograph.Style.OscillograPadding);

	// calculate oscillogram bb
	float PadTop = 0.0f, PadBottom = 0.0f, PadLeft = 0.0f, PadRight = 0.0f;
	const ImRect OscillogramRect = ImRect(CanvasRect.Min + ImVec2(PadLeft, PadTop), CanvasRect.Max - ImVec2(PadRight, PadBottom));

	// render frame
	const ImU32 FrameColor = COL_CONST32(UI::COLOR_OSCIL_FRAME_BACKGROUND);
	ImGui::RenderFrame(FrameRect.Min, FrameRect.Max, FrameColor, true, Style.FrameRounding);

	// render background
	const ImU32 AreaColor = COL_CONST32(UI::COLOR_OSCIL_AREA_BACKGROUND);
	DrawList.AddRectFilled(OscillogramRect.Min, OscillogramRect.Max, AreaColor);
}
