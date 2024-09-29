#include "Disassembler.h"

#include "AppDebugger.h"
#include "Utils/UI.h"
#include "Utils/Hotkey.h"
#include "Utils/Memory.h"
#include "Devices/CPU/Z80.h"
#include "Motherboard/Motherboard.h"

namespace
{
	static const char* ThisWindowName = TEXT("Disassembler");

	// set column widths
	static constexpr float ColumnWidth_Breakpoint = 2;
	static constexpr float ColumnWidth_PrefixAddress = 10.0f;
	static constexpr float ColumnWidth_Address = 50.0f;
	static constexpr float ColumnWidth_Mnemonic = 200.0f;
}

namespace Instruction
{
	static constexpr const char* Load = "LD";

	static constexpr const char* Registers8[]				= { "B", "C", "D", "E", "H", "L", "(HL)", "A"};
	static constexpr const char* Registers16[]				= { "BC", "DE", "HL", "SP", "AF"};
	static constexpr const char* Conditions[]				= { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M" };
	static constexpr const char* ArithmeticLogic[]			= { "ADD A,", "ADC A,", "SUB", "SBC A,", "AND", "XOR", "OR", "CP"};
	static constexpr const char* RotationShift[]			= { "RLC", "RRC", "RL", "RR", "SLA", "SRA", "SLL", "SRL"};
	static constexpr const char* InterruptModes[]			= { "0", "0/1", "1", "2", "0", "0/1", "1", "2" };
	static constexpr const char* BlockInstructions[4][4]	= { { "LDI", "CPI", "INI", "OUTI"},
															    { "LDD", "CPD", "IND", "OUTD" },
															    { "LDIR", "CPIR", "INIR", "OTIR" },
															    { "LDDR", "CPDR", "INDR", "OTDR" } };

	#define OCTAL_X(opcode)		((opcode & 0xC0) >> 6)
	#define OCTAL_Y(opcode)		((opcode & 0x38) >> 3)
	#define OCTAL_Z(opcode)		((opcode & 0x07) >> 0)
	#define OCTAL_P(opcode)		((opcode & 0x30) >> 4)
	#define OCTAL_Q(opcode)		((opcode & 0x08) >> 3)

	uint8_t UnprefixedOpcodes(uint8_t Opcode, uint32_t& Address, uint8_t* RawData, std::string& Mnemonic)
	{
		switch (OCTAL_X(Opcode))
		{
		case 0:
		{
			switch (OCTAL_Z(Opcode))
			{
			case 0:
			{
				switch (OCTAL_Y(Opcode))
				{
				case 0: Mnemonic = "NOP";									return 1;
				case 1:	Mnemonic = "EX AF, AF'";							return 1;
				case 2:
					{
						const int16_t Value = (signed char)RawData[Address++];
						Mnemonic = std::format("DJNZ #{:04X}", uint16_t(Value + Address));
						return 2;
					}
				case 3:
					{
						const int16_t Value = (signed char)RawData[Address++];
						Mnemonic = std::format("JR #{:04X}", uint16_t(Value + Address));
						return 2;
					}
				case 4:
				case 5:
				case 6:
				case 7:
					{
						const int16_t Value = (signed char)RawData[Address++];
						Mnemonic = std::format("JR {}, #{:04X}", Conditions[OCTAL_Y(Opcode) - 4], uint16_t(Value + Address));
						return 2;
					}
				}
			}
			case 1:
			{
				if (OCTAL_Q(Opcode) == 0)
				{
					const uint16_t Value = *reinterpret_cast<uint16_t*>(RawData + Address); Address += 2;
					Mnemonic = std::format("LD {}, #{:04X}", Registers16[OCTAL_P(Opcode)], Value);
					return 3;
				}
				else
				{
					Mnemonic = std::string("ADD HL, ") + Registers16[OCTAL_P(Opcode)];
					return 1;
				}
			}
			case 2:
			{
				if (OCTAL_Q(Opcode) == 0)
				{
					switch (OCTAL_P(Opcode))
					{
						case 0: Mnemonic = "LD (BC), A";	return 1;
						case 1: Mnemonic = "LD (DE), A";	return 1;
						case 2:
						{
							const uint16_t Value = *reinterpret_cast<uint16_t*>(RawData + Address); Address += 2;
							Mnemonic = std::format("LD (#{:04X}), HL", Value);
							return 3;
						}
						case 3:
						{
							const uint16_t Value = *reinterpret_cast<uint16_t*>(RawData + Address); Address += 2;
							Mnemonic = std::format("LD (#{:04X}), A", Value);
							return 3;
						}
					}
				}
				else
				{
					switch (OCTAL_P(Opcode))
					{
						case 0: Mnemonic = "LD A, (BC)";	return 1;
						case 1: Mnemonic = "LD A, (DE)";	return 1;
						case 2:
						{
							const uint16_t Value = *reinterpret_cast<uint16_t*>(RawData + Address); Address += 2;
							Mnemonic = std::format("LD HL, (#{:04X})", Value);
							return 3;
						}
						case 3:
						{
							const uint16_t Value = *reinterpret_cast<uint16_t*>(RawData + Address); Address += 2;
							Mnemonic = std::format("LD A, (#{:04X})", Value);
							return 3;
						}
					}
				}
			}
			case 3:
			{
				if (OCTAL_Q(Opcode) == 0)
				{
					Mnemonic = std::string("INC ") + Registers16[OCTAL_P(Opcode)];
					return 1;
				}
				else
				{
					Mnemonic = std::string("DEC ") + Registers16[OCTAL_P(Opcode)];
					return 1;
				}
			}
			case 4:
			{
				Mnemonic = std::string("INC ") + Registers8[OCTAL_Y(Opcode)];	return 1;
			}
			case 5:
			{
				Mnemonic = std::string("DEC ") + Registers8[OCTAL_Y(Opcode)];	return 1;
			}
			case 6:
			{
				const uint8_t Value = RawData[Address++];
				Mnemonic = std::format("LD {}, #{:02X}", Registers8[OCTAL_Y(Opcode)], Value);
				return 2;
			}
			case 7:
			{
				switch (OCTAL_Z(Opcode))
				{
				case 0: Mnemonic = std::string("RLCA");	return 1;
				case 1: Mnemonic = std::string("RRCA");	return 1;
				case 2: Mnemonic = std::string("RLA");	return 1;
				case 3: Mnemonic = std::string("RRA");	return 1;
				case 4: Mnemonic = std::string("DDA");	return 1;
				case 5: Mnemonic = std::string("CPL");	return 1;
				case 6: Mnemonic = std::string("SCF");	return 1;
				case 7: Mnemonic = std::string("CCF");	return 1;
				}
			}
			}
		}
		case 1:
			break;
		case 2:
			break;
		case 3:
			{
				switch (OCTAL_Z(Opcode))
				{
					case 0:
						Mnemonic = std::format("RET {}", Conditions[OCTAL_Y(Opcode)]);
						return 1;
					case 1:
						break;
					case 2:
						break;
					case 3:
					{
						switch (OCTAL_Y(Opcode))
						{
							case 0:
							{
								const uint16_t Value = *reinterpret_cast<uint16_t*>(RawData + Address); Address += 2;
								Mnemonic = std::format("JP  #{:04X}", Value);
								return 3;
							}
							case 1: return 0xCB;
							case 2:
							{
								const int8_t Value = RawData[Address++];
								Mnemonic = std::format("OUT (#{:02X}), A", Value);
								return 2;
							}
							case 3:
							{
								const int8_t Value = RawData[Address++];
								Mnemonic = std::format("IN A, (#{:02X})", Value);
								return 2;
							}
							case 4:
								Mnemonic = "EX (SP), HL";
								return 1;
							case 5:
								Mnemonic = "EX DE, HL";
								return 1;
							case 6:
								Mnemonic = "DI";
								return 1;
							case 7:
								Mnemonic = "EI";
								return 1;
						}
					}
				}
			}
		}
		//assert(false);
		Mnemonic = "***";
		return 1;
	}

	uint8_t Mnemonic(std::string& Mnemonic, uint32_t& Address, uint8_t* RawData)
	{
		const uint8_t Opcode = RawData[Address++];
		uint8_t Result = UnprefixedOpcodes(Opcode, Address, RawData, Mnemonic);
		switch (Result)
		{
		case 0xDD:
		default:
			return Result;
		}
	}
}

SDisassembler::SDisassembler(EFont::Type _FontName)
	: SWindow(ThisWindowName, _FontName, true)
	, LatestClockCounter(INDEX_NONE)
	, bMemoryArea(false)
	, CursorAtAddress(INDEX_NONE)
{}

void SDisassembler::Initialize()
{
}

void SDisassembler::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	{
		Input_HotKeys();

		const EThreadStatus Status = GetMotherboard().GetState<EThreadStatus>(NAME_MainBoard, NAME_None);
		if (Status == EThreadStatus::Stop)
		{
			const uint64_t ClockCounter = GetMotherboard().GetState<uint64_t>(NAME_MainBoard, NAME_None);
			if (ClockCounter != LatestClockCounter || ClockCounter == -1)
			{
				Update_MemorySnapshot();
				LatestClockCounter = ClockCounter;
			}
		}

		Draw_CodeDisassembler(Status);
	}
	ImGui::End();
}

FMotherboard& SDisassembler::GetMotherboard() const
{
	return *FAppFramework::Get<FAppDebugger>().Motherboard;
}

void SDisassembler::Update_MemorySnapshot()
{
	Snapshot = GetMotherboard().GetState<FMemorySnapshot>(NAME_MainBoard, NAME_Memory);
	Memory::ToAddressSpace(Snapshot, AddressSpace);
	if (CursorAtAddress = INDEX_NONE)
	{
		// ToDo read program counter from CPU
		CursorAtAddress = 0;
	}
}

void SDisassembler::Draw_CodeDisassembler(EThreadStatus Status)
{
	if (LatestClockCounter == INDEX_NONE)
	{
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_WEAK));
	UI::TextAligned(std::format("CC: #{:016X}", Snapshot.SnapshotTimeCC).c_str(), { 1.0f, 0.5f });
	ImGui::PopStyleColor();


	ImGui::BeginChild("##Scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration);
	ImGui::PushFont(FFonts::Get().GetFont(NAME_DISASSEMBLER_16));
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 5, 0 });
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

	if (ImGui::BeginTable("##Disassembler", bMemoryArea ? 4 : 3,
		ImGuiTableFlags_NoPadOuterX |
		ImGuiTableFlags_NoClip |
		ImGuiTableFlags_NoBordersInBodyUntilResize |
		ImGuiTableFlags_Resizable
	))
	{
		ImGui::TableSetupColumn("Breakpoint", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_Breakpoint);
		if (bMemoryArea) ImGui::TableSetupColumn("PrefixAddress", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_PrefixAddress);
		ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_Address);
		ImGui::TableSetupColumn("Mnemonic", ImGuiTableColumnFlags_WidthStretch , ColumnWidth_Mnemonic);

		const int32_t Lines = UI::GetVisibleLines(FontName)+10;
		uint32_t Address = CursorAtAddress;
		for (int32_t i = 0; i < Lines; ++i)
		//while(true)
		{
			ImGui::TableNextRow();

			Draw_Breakpoint(Address);
			Draw_Address(Address);
			Draw_Mnemonic(Address);
		}
		ImGui::EndTable();
	}
	ImGui::PopStyleVar(2);
	ImGui::PopFont();
	ImGui::EndChild();
}

void SDisassembler::Draw_Breakpoint(uint32_t Address)
{
	ImGui::TableNextColumn();
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BREAKPOINT));
	//ImGui::TextColored(COL_CONST(UI::COLOR_REGISTERS), "");
}

void SDisassembler::Draw_Address(uint32_t Address)
{
	if (bMemoryArea)
	{
		ImGui::TableNextColumn();
		ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BG_PREFIX_ADDRESS));
		ImGui::TextColored(COL_CONST(UI::COLOR_DISASM_ADDRESS), "00");
	}
	
	ImGui::TableNextColumn();
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BG_ADDRESS));
	ImGui::TextColored(COL_CONST(UI::COLOR_DISASM_ADDRESS), "#%.4X", Address);
}

uint32_t SDisassembler::Draw_Mnemonic(uint32_t& Address)
{
	std::string Mnemonic;
	const uint32_t Length = Instruction::Mnemonic(Mnemonic, Address, AddressSpace.data());

	ImGui::TableNextColumn(); ImGui::SameLine(30);
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BG_MNEMONIC));
	ImGui::TextColored(COL_CONST(UI::COLOR_DISASM_MNEMONIC), Mnemonic.c_str());
	return Length;
}

void SDisassembler::Input_HotKeys()
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
