#include "CPU_State.h"

#include "AppDebugger.h"
#include "Utils/Hotkey.h"
#include "Motherboard/Motherboard.h"

namespace
{
	static const char* ThisWindowName = TEXT("CPU State");

	// set column widths
	static constexpr float ColumnWidth_Register = 20;
	static constexpr float ColumnWidth_RegisterValue = 60.0f;
}

SCPU_State::SCPU_State(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(FontName)
		.SetIncludeInWindows(true))
{}

void SCPU_State::Initialize()
{
	HighlightRegisters =
	{
		{	/*0*/ {"PC",	(uint8_t*)&LatestRegistersState.PC.Word,	UI::COLOR_CPU_VALUE,	INDEX_NONE,				true},
			/*1*/ {"HL",	(uint8_t*)&LatestRegistersState.HL.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*2*/ {nullptr,	(uint8_t*)&LatestRegistersState.HL.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*3*/ {"HL'",	(uint8_t*)&LatestRegistersState.HL_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*4*/ {nullptr,	(uint8_t*)&LatestRegistersState.HL_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false}	},

		{	/*0*/ {"SP",	(uint8_t*)&LatestRegistersState.SP.Word,	UI::COLOR_CPU_VALUE,	INDEX_NONE,				true},
			/*1*/ {"DE",	(uint8_t*)&LatestRegistersState.DE.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*2*/ {nullptr,	(uint8_t*)&LatestRegistersState.DE.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*3*/ {"DE'",	(uint8_t*)&LatestRegistersState.DE_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*4*/ {nullptr,	(uint8_t*)&LatestRegistersState.DE_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false}	},

		{	/*0*/ {"IX",	(uint8_t*)&LatestRegistersState.IX.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*1*/ {nullptr,	(uint8_t*)&LatestRegistersState.IX.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*2*/ {"BC",	(uint8_t*)&LatestRegistersState.BC.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*3*/ {nullptr,	(uint8_t*)&LatestRegistersState.BC.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*4*/ {"BC'",	(uint8_t*)&LatestRegistersState.BC_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*5*/ {nullptr,	(uint8_t*)&LatestRegistersState.BC_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false}	},

		{	/*0*/ {"IY",	(uint8_t*)&LatestRegistersState.IY.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*1*/ {nullptr,	(uint8_t*)&LatestRegistersState.IY.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*2*/ {"AF",	(uint8_t*)&LatestRegistersState.AF.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*3*/ {nullptr,	(uint8_t*)&LatestRegistersState.AF.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*4*/ {"AF'",	(uint8_t*)&LatestRegistersState.AF_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false},
			/*5*/ {nullptr,	(uint8_t*)&LatestRegistersState.AF_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	false}	},
	};
}

void SCPU_State::Tick(float DeltaTime)
{
	Status = GetMotherboard().GetState<EThreadStatus>(NAME_MainBoard, NAME_None);
	if (Status == EThreadStatus::Stop)
	{
		const uint64_t ClockCounter = GetMotherboard().GetState<uint64_t>(NAME_MainBoard, NAME_None);
		if (ClockCounter != LatestClockCounter)
		{
			Update_Registers();
			LatestClockCounter = ClockCounter;
		}
	}
}

void SCPU_State::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	{
		Input_HotKeys();
		Draw_States(Status == EThreadStatus::Stop);
	}
	ImGui::End();
}

FMotherboard& SCPU_State::GetMotherboard() const
{
	return *FAppFramework::Get<FAppDebugger>().Motherboard;
}

void SCPU_State::Update_Registers()
{
	const FRegisters RegistersState = GetMotherboard().GetState<FRegisters>(NAME_MainBoard, NAME_Z80);
	HighlightRegisters[0][0].H_Color = RegistersState.PC    == LatestRegistersState.PC		? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[0][1].H_Color = RegistersState.HL.H  == LatestRegistersState.HL.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[0][2].L_Color = RegistersState.HL.L  == LatestRegistersState.HL.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[0][3].H_Color = RegistersState.HL_.H == LatestRegistersState.HL_.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[0][4].L_Color = RegistersState.HL_.L == LatestRegistersState.HL_.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	HighlightRegisters[1][0].H_Color = RegistersState.SP    == LatestRegistersState.SP		? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[1][1].H_Color = RegistersState.DE.H  == LatestRegistersState.DE.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[1][2].L_Color = RegistersState.DE.L  == LatestRegistersState.DE.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[1][3].H_Color = RegistersState.DE_.H == LatestRegistersState.DE_.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[1][4].L_Color = RegistersState.DE_.L == LatestRegistersState.DE_.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	HighlightRegisters[2][0].H_Color = RegistersState.IX.H  == LatestRegistersState.IX.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][1].L_Color = RegistersState.IX.L  == LatestRegistersState.IX.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][2].H_Color = RegistersState.BC.H  == LatestRegistersState.BC.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][3].L_Color = RegistersState.BC.L  == LatestRegistersState.BC.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][4].H_Color = RegistersState.BC_.H == LatestRegistersState.BC_.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][5].L_Color = RegistersState.BC_.L == LatestRegistersState.BC_.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	HighlightRegisters[3][0].H_Color = RegistersState.IY.H  == LatestRegistersState.IY.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][1].L_Color = RegistersState.IY.L  == LatestRegistersState.IY.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][2].H_Color = RegistersState.AF.H  == LatestRegistersState.AF.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][3].L_Color = RegistersState.AF.L  == LatestRegistersState.AF.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][4].H_Color = RegistersState.AF_.H == LatestRegistersState.AF_.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][5].L_Color = RegistersState.AF_.L == LatestRegistersState.AF_.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	LatestRegistersState = RegistersState;
}

void SCPU_State::Draw_States(bool bEnabled)
{
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 5.0f, 0.0f });
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });

	Draw_Registers();

	ImGui::PopStyleVar(2);
}

void SCPU_State::Draw_Registers()
{
	if (ImGui::BeginTable("##CPU Registers", 6,
		ImGuiTableFlags_NoPadOuterX | 
		ImGuiTableFlags_SizingStretchSame |
		ImGuiTableFlags_NoPadInnerX))
	{
		ImGui::PushFont(FFonts::Get().GetFont(NAME_DOS_14));
		const uint16_t* CurrentState = reinterpret_cast<const uint16_t*>(&LatestRegistersState);
		ImGui::TableSetupColumn("RegistersName_GroupA", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Register);
		ImGui::TableSetupColumn("RegistersValueA", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_RegisterValue);
		ImGui::TableSetupColumn("RegistersName_GroupB", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Register);
		ImGui::TableSetupColumn("RegistersValueB", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_RegisterValue);
		ImGui::TableSetupColumn("RegistersName_GroupC", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Register);
		ImGui::TableSetupColumn("RegistersValueC", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_RegisterValue);

		for (std::vector<FRegisterVisual>& Visual : HighlightRegisters)
		{
			Draw_Registers_Row(Visual);
		}

		ImGui::PopFont();
		ImGui::EndTable();
	}
}

void SCPU_State::Draw_Registers_Row(const std::vector<FRegisterVisual>& RowVisual)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	for (const FRegisterVisual& Visual : RowVisual)
	{
		if (Visual.Name)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_CPU_REGISTER));
			ImGui::Text(Visual.Name);
			ImGui::PopStyleColor();

			ImGui::TableNextColumn();

			ImGui::TextColored(COL_CONST(UI::COLOR_CPU_SYMBOL), "#");
			ImGui::SameLine(0, 0);
			ImGui::TextColored(COL_CONST(Visual.H_Color), std::format("{:02X}", *(Visual.Value + 1)).c_str());

			if (!Visual.bWord)
			{
				continue;
			}
		}
		ImGui::SameLine(0, 0);
		ImGui::TextColored(COL_CONST(Visual.L_Color == INDEX_NONE ? Visual.H_Color : Visual.L_Color), std::format("{:02X}", *(Visual.Value + 0)).c_str());
		ImGui::TableNextColumn();
	}
}

void SCPU_State::Input_HotKeys()
{
}
