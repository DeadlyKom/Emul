#include "CPU_State.h"

#include "AppDebugger.h"
#include "Utils/Hotkey.h"
#include "Motherboard/Motherboard.h"

namespace
{
	static const char* ThisWindowName = TEXT("CPU State");
}

SCPU_State::SCPU_State()
	: SWindow(ThisWindowName, true)
{}

void SCPU_State::Initialize()
{
	for (int32_t i = 0; i < FRegisters::PrimaryRegistrars; ++i)
	{
		RegisterColor[i] = &UI::COLOR_NUMBER;
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

	Input_HotKeys();

	const EThreadStatus Status = GetMotherboard().GetState<EThreadStatus>(NAME_MainBoard, NAME_None);
	if (Status == EThreadStatus::Stop)
	{
		Update_Registers();
	}

	DrawStates(Status == EThreadStatus::Stop);

	ImGui::End();
}

FMotherboard& SCPU_State::GetMotherboard() const
{
	return *FAppFramework::Get<FAppDebugger>().Motherboard;
}

void SCPU_State::Update_Registers()
{
	const FRegisters RegistersState = GetMotherboard().GetState<FRegisters>(NAME_MainBoard, NAME_Z80);
	const uint16_t* LatestState = reinterpret_cast<const uint16_t*>(&LatestRegistersState);
	const uint16_t* CurrentState = reinterpret_cast<const uint16_t*>(&RegistersState);

	for (int32_t i = 0; i < FRegisters::PrimaryRegistrars; ++i)
	{
		RegisterColor[i] = LatestState[i] == CurrentState[i] ? &UI::COLOR_NUMBER : &UI::COLOR_NUMBER_UPDATED;
	}
	LatestRegistersState = RegistersState;
}

void SCPU_State::DrawStates(bool bEnabled)
{
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 5.0f, 0.0f });
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, 0.0f });

	static ImGuiTableFlags Flags =
		ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingStretchSame |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Hideable |
		ImGuiTableFlags_ContextMenuInBody;

	UI::DrawTable("CPU States", Flags, bEnabled, {
		{ "Regs", ImGuiTableColumnFlags_WidthFixed, 80, 0, std::bind(&ThisClass::Column_DrawRegisters, this) },
		});

	ImGui::PopStyleVar(2);
}

void SCPU_State::Column_DrawRegisters()
{
	static ImGuiTableFlags flags =
		ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_SizingStretchSame |
		ImGuiTableFlags_NoPadInnerX |
		ImGuiTableFlags_ContextMenuInBody;
	if (ImGui::BeginTable("Registers_", 2, flags))
	{
		const uint16_t* CurrentState = reinterpret_cast<const uint16_t*>(&LatestRegistersState);
		ImGui::TableSetupColumn("RegistersNames", ImGuiTableColumnFlags_WidthFixed, 30);
		for (int32_t i = 0; i < FRegisters::PrimaryRegistrars; ++i)
		{
			UI::DrawProperty(FRegisters::RegistersName[i], std::format("#{:04X}", CurrentState[i]).c_str(), nullptr, *RegisterColor[i]);
		}
		ImGui::EndTable();
	}
}

void SCPU_State::Input_HotKeys()
{
	static std::vector<FHotKey> Hotkeys =
	{
		{ ImGuiKey_F4,	ImGuiInputFlags_Repeat,	[this]() { GetMotherboard().Input_Step(FCPU_StepType::StepTo);		}},	// debugger: step into
		{ ImGuiKey_F7,	ImGuiInputFlags_Repeat,	[this]() { GetMotherboard().Input_Step(FCPU_StepType::StepInto);	}},	// debugger: step into
		{ ImGuiKey_F8,	ImGuiInputFlags_Repeat,	[this]() { GetMotherboard().Input_Step(FCPU_StepType::StepOver);	}},	// debugger: step over
		{ ImGuiKey_F11,	ImGuiInputFlags_Repeat,	[this]() { GetMotherboard().Input_Step(FCPU_StepType::StepOut);		}},	// debugger: step out
	};

	Shortcut::Handler(Hotkeys);
}
