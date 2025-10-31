#include "MemoryDump.h"

#include "AppDebugger.h"
#include <Utils/UI/Draw.h>
#include "Utils/Hotkey.h"
#include "Utils/Memory.h"
#include "Motherboard/Motherboard.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Memory dump";

	#define FORMAT_ADDRESS(upper)		(upper ? "%04X" : "%04x")

	// set column widths
	static constexpr float ColumnWidth_Breakpoint = 2;
	static constexpr float ColumnWidth_PrefixAddress = 10.0f;
}

SMemoryDump::SMemoryDump(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
	, bMemoryArea(false)
	, bASCII_Values(true)
	, bShowStatusBar(true)
	, ColumnsToDisplay(16)
	, BaseDisplayAddress(0)
	, MemoryDumpScale(1.0f)
	, LatestClockCounter(INDEX_NONE)
	, Status(EThreadStatus::Unknown)
{}

void SMemoryDump::Tick(float DeltaTime)
{
	Status = GetMotherboard().GetState<EThreadStatus>(NAME_MainBoard, NAME_None);
	if (Status == EThreadStatus::Stop)
	{
		const uint64_t ClockCounter = GetMotherboard().GetState<uint64_t>(NAME_MainBoard, NAME_None);
		if (ClockCounter != LatestClockCounter || ClockCounter == INDEX_NONE)
		{
			Load_MemorySnapshot();
			LatestClockCounter = ClockCounter;
		}
	}
}

void SMemoryDump::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen);
	{
		Input_HotKeys();
		Input_Mouse();
		Draw_DumpMemory();

		ImGui::End();
	}
}

FMotherboard& SMemoryDump::GetMotherboard() const
{
	return *FAppFramework::Get<FAppDebugger>().Motherboard;
}

float SMemoryDump::InaccessibleHeight(int32_t LineNum, int32_t SeparatorNum) const
{
	float FooterHeight = 0;
	const float HeightSeparator = ImGui::GetStyle().ItemSpacing.y;
	if (bShowStatusBar)
	{
		FooterHeight += HeightSeparator * (float)SeparatorNum + ImGui::GetFrameHeightWithSpacing() * (float)LineNum;
	}

	return FooterHeight;
}

void SMemoryDump::Draw_DumpMemory()
{
	if (ImGui::BeginChild("##MemoryDumpHeader", ImVec2(0, -InaccessibleHeight(1, 0)), false, ImGuiWindowFlags_None))
	{
		ImGui::PushFont(FFonts::Get().GetFont(FontName));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 0.0f, 0.0f });
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 2.0f });

		{
			//const float TextHeight = ImGui::GetTextLineHeight();
			const float GlyphWidth = ImGui::CalcTextSize("F").x;
			const float HexCellWidth = (float)(int)(GlyphWidth * 2.5f);
			//const float SpacingBetweenMidCols = (float)(int)(HexCellWidth * 0.25f);
			const float PosHexStart = (4 + 2) * GlyphWidth;
			//const float PosHexEnd = PosHexStart + (HexCellWidth * ColumnsToDisplay);
			//const float PosAsciiStart = PosHexEnd + GlyphWidth * 1;
			//const float PosAsciiEnd = PosAsciiStart + ColumnsToDisplay * GlyphWidth;

			for (int Column = 0; Column < ColumnsToDisplay; ++Column)
			{
				float byte_pos_x = PosHexStart + HexCellWidth * Column;
				ImGui::SameLine(byte_pos_x);
				ImGui::Text("%02X ", Column);
			}
		}

		ImGui::Separator();

		if (ImGui::BeginChild("##MemoryDumpScrolling", ImVec2(0, -InaccessibleHeight(0, 2)), false,
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoNav))
		{
			ImGui::PushFont(FFonts::Get().GetFont(FontName));
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 0.0f, 0.0f });
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });

			MemoryDumpID = ImGui::GetCurrentWindow()->ID;

			{
				ImDrawList* DrawList = ImGui::GetWindowDrawList();

				const int LineTotalCount = (int32_t)((AddressSpace.size() + ColumnsToDisplay - 1) / ColumnsToDisplay);
				const float TextHeight = ImGui::GetTextLineHeight() - MemoryDumpScale;
				const float GlyphWidth = ImGui::CalcTextSize("F").x;
				const float HexCellWidth = (float)(int)(GlyphWidth * 2.5f);
				const float SpacingBetweenMidCols = (float)(int)(HexCellWidth * 0.25f);
				const float PosHexStart = (4 + 2) * GlyphWidth;
				const float PosHexEnd = PosHexStart + (HexCellWidth * ColumnsToDisplay);
				const float PosAsciiStart = PosHexEnd + GlyphWidth * 1;
				const float PosAsciiEnd = PosAsciiStart + ColumnsToDisplay * GlyphWidth;

				ImGuiListClipper Clipper;
				Clipper.Begin(LineTotalCount, TextHeight);
				while (Clipper.Step())
				{
					for (int Line = Clipper.DisplayStart; Line < Clipper.DisplayEnd; ++Line)
					{
						int32_t Address = (int32_t)(Line * ColumnsToDisplay);
						ImGui::Text(FORMAT_ADDRESS(true), BaseDisplayAddress + Address);

						for (int Column = 0; Column < ColumnsToDisplay && Address < AddressSpace.size(); ++Column, ++Address)
						{
							float byte_pos_x = PosHexStart + HexCellWidth * Column;
							ImGui::SameLine(byte_pos_x);

							uint8_t& MemoryValue = AddressSpace[Address];

							if (MemoryValue == 0)
							{
								ImGui::TextDisabled("00 ");
							}
							else
							{
								ImGui::Text("%02X ", MemoryValue);
							}
						}

						// draw ASCII values
						if (bASCII_Values)
						{
							ImGui::SameLine(PosAsciiStart);
							ImVec2 Pos = ImGui::GetCursorScreenPos();
							Address = Line * ColumnsToDisplay;
							ImGui::PushID(Line);
							if (ImGui::InvisibleButton("ascii", ImVec2(PosAsciiEnd - PosAsciiStart, TextHeight)))
							{
								DataEditingAddress = DataPreviewAddress = Address + (int32_t)((ImGui::GetIO().MousePos.x - Pos.x) / GlyphWidth);
								DataEditingTakeFocus = true;
							}
							ImGui::PopID();
							for (int Column = 0; Column < ColumnsToDisplay && Address < AddressSpace.size(); ++Column, ++Address)
							{
								if (Address == DataEditingAddress)
								{
									DrawList->AddRectFilled(Pos, ImVec2(Pos.x + GlyphWidth, Pos.y + TextHeight), ImGui::GetColorU32(ImGuiCol_FrameBg));
									DrawList->AddRectFilled(Pos, ImVec2(Pos.x + GlyphWidth, Pos.y + TextHeight), ImGui::GetColorU32(ImGuiCol_TextSelectedBg));
								}

								unsigned char Char = AddressSpace[Address];
								char DisplayChar = (Char < 32 || Char >= 128) ? '.' : Char;
								const float CharhWidth = ImGui::CalcTextSize(&DisplayChar, &DisplayChar + 1).x;
								DrawList->AddText(Pos + ImVec2((GlyphWidth - CharhWidth) * 0.9f, 0.0f), (DisplayChar == Char) ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled), &DisplayChar, &DisplayChar + 1);
								Pos.x += GlyphWidth;
							}
						}
					}
				}
			}
			ImGui::PopStyleVar(2);
			ImGui::PopFont();
		}

		ImGui::EndChild();
		ImGui::Separator();
		ImGui::PopStyleVar(2);
		ImGui::PopFont();
	}
	ImGui::EndChild();

	// draw current scale
	{
		ImGui::PushFont(FFonts::Get().GetFont(NAME_DOS_12));
		ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_WEAK));
		UI::TextAligned(std::format("{}%", 100.0f * MemoryDumpScale).c_str(), { 1.0f, 0.5f });
		ImGui::PopStyleColor();
		ImGui::PopFont();
	}
}

void SMemoryDump::Input_HotKeys()
{
}

void SMemoryDump::Input_Mouse()
{
	ImGuiContext& Context = *GImGui;
	const float MouseWheel = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner) ? Context.IO.MouseWheel : 0.0f;

	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != MemoryDumpID)
	{
		return;
	}

	if (MouseWheel != 0.0f && Context.IO.KeyCtrl && !Context.IO.FontAllowUserScaling)
	{
		MemoryDumpScale += MouseWheel * 0.0625f;
		MemoryDumpScale = FFonts::Get().SetSize(FontName, MemoryDumpScale, 0.5f, 1.5f);
	}
}

void SMemoryDump::Load_MemorySnapshot()
{
	Snapshot = GetMotherboard().GetState<FMemorySnapshot>(NAME_MainBoard, NAME_Memory);
	Memory::ToAddressSpace(Snapshot, AddressSpace);
}
