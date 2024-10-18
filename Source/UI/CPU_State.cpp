#include "CPU_State.h"

#include "AppDebugger.h"
#include "Utils/Hotkey.h"
#include "Motherboard/Motherboard.h"

namespace
{
	static const char* ThisWindowName = TEXT("CPU State");

	// set column widths
	static constexpr float ColumnWidth_Flags = 8;
	static constexpr float ColumnWidth_Register = 20;
	static constexpr float ColumnWidth_Register_ = 30;
	static constexpr float ColumnWidth_RegisterValue = 60.0f;
	static constexpr float ColumnWidth_RegisterValue_ = 80.0f;
}

SCPU_State::SCPU_State(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
	, LatestFlags(0)
	, NewFlags(0)
{}

void SCPU_State::Initialize()
{
	HighlightRegisters =
	{
		{	/*0*/ {"PC",	(uint8_t*)&LatestRegistersState.PC.Word,	UI::COLOR_CPU_VALUE,	INDEX_NONE,				ERegisterVisualType::WORD},
			/*1*/ {"HL",	(uint8_t*)&LatestRegistersState.HL.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*2*/ {nullptr,	(uint8_t*)&LatestRegistersState.HL.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*3*/ {"HL'",	(uint8_t*)&LatestRegistersState.HL_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*4*/ {nullptr,	(uint8_t*)&LatestRegistersState.HL_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},

			/*5*/ {"IM",	(uint8_t*)&LatestRegistersState.IM,			UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::INT}, },

		{	/*0*/ {"SP",	(uint8_t*)&LatestRegistersState.SP.Word,	UI::COLOR_CPU_VALUE,	INDEX_NONE,				ERegisterVisualType::WORD},
			/*1*/ {"DE",	(uint8_t*)&LatestRegistersState.DE.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*2*/ {nullptr,	(uint8_t*)&LatestRegistersState.DE.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*3*/ {"DE'",	(uint8_t*)&LatestRegistersState.DE_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*4*/ {nullptr,	(uint8_t*)&LatestRegistersState.DE_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},

			/*5*/ {"IFF1",	(uint8_t*)&LatestRegistersState.bIFF1,		UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BOOL},	},

		{	/*0*/ {"IX",	(uint8_t*)&LatestRegistersState.IX.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*1*/ {nullptr,	(uint8_t*)&LatestRegistersState.IX.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*2*/ {"BC",	(uint8_t*)&LatestRegistersState.BC.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*3*/ {nullptr,	(uint8_t*)&LatestRegistersState.BC.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*4*/ {"BC'",	(uint8_t*)&LatestRegistersState.BC_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*5*/ {nullptr,	(uint8_t*)&LatestRegistersState.BC_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},

			/*6*/ {"IFF2",	(uint8_t*)&LatestRegistersState.bIFF2,		UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BOOL},	},

		{	/*0*/ {"IY",	(uint8_t*)&LatestRegistersState.IY.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*1*/ {nullptr,	(uint8_t*)&LatestRegistersState.IY.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*2*/ {"AF",	(uint8_t*)&LatestRegistersState.AF.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*3*/ {nullptr,	(uint8_t*)&LatestRegistersState.AF.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*4*/ {"AF'",	(uint8_t*)&LatestRegistersState.AF_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*5*/ {nullptr,	(uint8_t*)&LatestRegistersState.AF_.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},

			/*6*/ {"IR",	(uint8_t*)&LatestRegistersState.IR.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},
			/*7*/ {nullptr,	(uint8_t*)&LatestRegistersState.IR.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::BYTE},	},
			///*6*/ {"Flags",	(uint8_t*)&LatestRegistersState.AF.Word,	UI::COLOR_CPU_VALUE,	UI::COLOR_CPU_VALUE,	ERegisterVisualType::FLAGS},},
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

		ImGui::End();
	}
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

	HighlightRegisters[0][5].L_Color = RegistersState.IM == LatestRegistersState.IM			? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	
	HighlightRegisters[1][0].H_Color = RegistersState.SP    == LatestRegistersState.SP		? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[1][1].H_Color = RegistersState.DE.H  == LatestRegistersState.DE.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[1][2].L_Color = RegistersState.DE.L  == LatestRegistersState.DE.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[1][3].H_Color = RegistersState.DE_.H == LatestRegistersState.DE_.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[1][4].L_Color = RegistersState.DE_.L == LatestRegistersState.DE_.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	HighlightRegisters[1][5].H_Color = RegistersState.bIFF1 == LatestRegistersState.bIFF1	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	HighlightRegisters[2][0].H_Color = RegistersState.IX.H  == LatestRegistersState.IX.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][1].L_Color = RegistersState.IX.L  == LatestRegistersState.IX.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][2].H_Color = RegistersState.BC.H  == LatestRegistersState.BC.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][3].L_Color = RegistersState.BC.L  == LatestRegistersState.BC.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][4].H_Color = RegistersState.BC_.H == LatestRegistersState.BC_.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[2][5].L_Color = RegistersState.BC_.L == LatestRegistersState.BC_.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	HighlightRegisters[2][6].H_Color = RegistersState.bIFF2 == LatestRegistersState.bIFF2	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	HighlightRegisters[3][0].H_Color = RegistersState.IY.H  == LatestRegistersState.IY.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][1].L_Color = RegistersState.IY.L  == LatestRegistersState.IY.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][2].H_Color = RegistersState.AF.H  == LatestRegistersState.AF.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][3].L_Color = RegistersState.AF.L  == LatestRegistersState.AF.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][4].H_Color = RegistersState.AF_.H == LatestRegistersState.AF_.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][5].L_Color = RegistersState.AF_.L == LatestRegistersState.AF_.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	HighlightRegisters[3][6].H_Color = RegistersState.IR.H	== LatestRegistersState.IR.H	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;
	HighlightRegisters[3][7].L_Color = RegistersState.IR.L	== LatestRegistersState.IR.L	? UI::COLOR_CPU_VALUE : UI::COLOR_CPU_VALUE_CHANGED;

	LatestFlags = NewFlags;
	NewFlags = RegistersState.AF.L.Byte;
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
	ImGui::BeginChild("##Scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration);
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 5.0f, 0.0f });
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

	ImGui::Dummy(ImVec2(0, 5));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 3));

	if (ImGui::BeginTable("##CPU Registers", 10,
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
		ImGui::TableSetupColumn("RegistersName_GroupD", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Register_);
		ImGui::TableSetupColumn("RegistersValueD", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_RegisterValue_);
		ImGui::TableSetupColumn("RegistersName_GroupE", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Register);
		ImGui::TableSetupColumn("RegistersValueE", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_RegisterValue);

		for (std::vector<FRegisterVisual>& Visual : HighlightRegisters)
		{
			Draw_Registers_Row(Visual);
		}

		ImGui::PopFont();
		ImGui::EndTable();
	}

	ImGui::Dummy(ImVec2(0, 3));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0, 5));

	if (ImGui::BeginTable("##CPU Flags", 8 + 1,
		ImGuiTableFlags_NoPadOuterX | 
		ImGuiTableFlags_SizingStretchSame |
		ImGuiTableFlags_NoPadInnerX))
	{
		ImGui::PushFont(FFonts::Get().GetFont(NAME_DOS_14));
		const uint16_t* CurrentState = reinterpret_cast<const uint16_t*>(&LatestRegistersState);
		ImGui::TableSetupColumn("Indent", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, (ColumnWidth_RegisterValue + ColumnWidth_Register) * 2 + ColumnWidth_RegisterValue_ + 3);
		ImGui::TableSetupColumn("FlagName_Sign", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Flags);
		ImGui::TableSetupColumn("FlagName_Zero", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Flags);
		ImGui::TableSetupColumn("FlagName_Y", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Flags);
		ImGui::TableSetupColumn("FlagName_HalfCarry", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Flags);
		ImGui::TableSetupColumn("FlagName_X", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Flags);
		ImGui::TableSetupColumn("FlagName_ParityOverflow", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Flags);
		ImGui::TableSetupColumn("FlagName_AddSubtract", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Flags);
		ImGui::TableSetupColumn("FlagName_Carry", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, ColumnWidth_Flags);

		const Register8& OldFlag = LatestFlags;
		const Register8& NewFlag = NewFlags;

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGui::TableNextColumn();
		const ImVec4 Sign = COL_CONST(NewFlag.Sign != OldFlag.Sign ? UI::COLOR_CPU_VALUE_CHANGED : UI::COLOR_CPU_VALUE);
		ImGui::TextColored(Sign, NewFlag.Sign						? "S" : "s");

		ImGui::TableNextColumn();
		const ImVec4 Zero = COL_CONST(NewFlag.Zero != OldFlag.Zero ? UI::COLOR_CPU_VALUE_CHANGED : UI::COLOR_CPU_VALUE);
		ImGui::TextColored(Zero, NewFlag.Zero						? "Z" : "z");

		ImGui::TableNextColumn();
		const ImVec4 Y = COL_CONST(NewFlag.Y != OldFlag.Y ? UI::COLOR_CPU_VALUE_CHANGED : UI::COLOR_CPU_VALUE);
		ImGui::TextColored(Y, NewFlag.Y								? "Y" : "y");

		ImGui::TableNextColumn();
		const ImVec4 Half_Carry = COL_CONST(NewFlag.Half_Carry != OldFlag.Half_Carry ? UI::COLOR_CPU_VALUE_CHANGED : UI::COLOR_CPU_VALUE);
		ImGui::TextColored(Half_Carry, NewFlag.Half_Carry			? "H" : "h");

		ImGui::TableNextColumn();
		const ImVec4 X = COL_CONST(NewFlag.X != OldFlag.X ? UI::COLOR_CPU_VALUE_CHANGED : UI::COLOR_CPU_VALUE);
		ImGui::TextColored(X, NewFlag.X								? "X" : "x");

		ImGui::TableNextColumn();
		const ImVec4 Parity_Overflow = COL_CONST(NewFlag.Parity_Overflow != OldFlag.Parity_Overflow ? UI::COLOR_CPU_VALUE_CHANGED : UI::COLOR_CPU_VALUE);
		ImGui::TextColored(Parity_Overflow, NewFlag.Parity_Overflow ? "P" : "p");

		ImGui::TableNextColumn();
		const ImVec4 Add_Subtract = COL_CONST(NewFlag.Add_Subtract != OldFlag.Add_Subtract ? UI::COLOR_CPU_VALUE_CHANGED : UI::COLOR_CPU_VALUE);
		ImGui::TextColored(Add_Subtract, NewFlag.Add_Subtract		? "N" : "n");

		ImGui::TableNextColumn();
		const ImVec4 Carry = COL_CONST(NewFlag.Carry != OldFlag.Carry ? UI::COLOR_CPU_VALUE_CHANGED : UI::COLOR_CPU_VALUE);
		ImGui::TextColored(Carry, NewFlag.Carry						? "C" : "c");

		ImGui::PopFont();
		ImGui::EndTable();
	}

	ImGui::PopStyleVar(2);
	ImGui::EndChild();
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

			if (Visual.ValueType == ERegisterVisualType::BOOL)
			{
				ImGui::SameLine(0, 0);
				ImGui::TextColored(COL_CONST(Visual.H_Color), std::format("{}", *(Visual.Value + 1) ? "TRUE" : "FALSE").c_str());
			}
			else if (Visual.ValueType == ERegisterVisualType::INT)
			{
				ImGui::SameLine(0, 0);
				ImGui::TextColored(COL_CONST(Visual.L_Color), std::format("{}", *(Visual.Value + 0)).c_str());
			}
			else
			{
				ImGui::TextColored(COL_CONST(UI::COLOR_CPU_SYMBOL), "#");
				ImGui::SameLine(0, 0);
				ImGui::TextColored(COL_CONST(Visual.H_Color), std::format("{:02X}", *(Visual.Value + 1)).c_str());
			}

			if (Visual.ValueType != ERegisterVisualType::WORD)
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
