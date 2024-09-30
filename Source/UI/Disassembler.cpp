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
	static constexpr float ColumnWidth_Instruction = 200.0f;
}

namespace Disassembler
{
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

	enum class ESyntacticType
	{
		Register		= UI::COLOR_DISASM_REGISTER,
		Condition		= UI::COLOR_DISASM_CONDITION,
		Constant		= UI::COLOR_DISASM_CONSTANT,
		ConstantOffset	= UI::COLOR_DISASM_CONSTANT_OFFSET,
		Symbol			= UI::COLOR_DISASM_SYMBOL,
		NotInit			= INDEX_NONE-1,
		None			= INDEX_NONE,
	};

	#define OCTAL_X(opcode)		((opcode & 0xC0) >> 6)
	#define OCTAL_Y(opcode)		((opcode & 0x38) >> 3)
	#define OCTAL_Z(opcode)		((opcode & 0x07) >> 0)
	#define OCTAL_P(opcode)		((opcode & 0x30) >> 4)
	#define OCTAL_Q(opcode)		((opcode & 0x08) >> 3)

	uint8_t UnprefixedOpcodes(uint16_t& Address, const uint8_t* RawData, std::string& Instruction)
	{
		const uint8_t Opcode = RawData[Address++];
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
				case 0: Instruction = "NOP";		return 1;
				case 1:	Instruction = "EX AF, AF'";	return 1;
				case 2:
					{
						const int16_t Value = (signed char)RawData[Address++];
						Instruction = std::format("DJNZ #${:04X}", uint16_t(Value + Address));
						return 2;
					}
				case 3:
					{
						const int16_t Value = (signed char)RawData[Address++];
						Instruction = std::format("JR #${:04X}", uint16_t(Value + Address));
						return 2;
					}
				case 4:
				case 5:
				case 6:
				case 7:
					{
						const int16_t Value = (signed char)RawData[Address++];
						Instruction = std::format("JR !{}, #${:04X}", Conditions[OCTAL_Y(Opcode) - 4], uint16_t(Value + Address));
						return 2;
					}
				}
			}
			case 1:
			{
				if (OCTAL_Q(Opcode) == 0)
				{
					const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
					Instruction = std::format("LD {}, #${:04X}", Registers16[OCTAL_P(Opcode)], Value);
					return 3;
				}
				else
				{
					Instruction = std::string("ADD HL, ") + Registers16[OCTAL_P(Opcode)];
					return 1;
				}
			}
			case 2:
			{
				if (OCTAL_Q(Opcode) == 0)
				{
					switch (OCTAL_P(Opcode))
					{
						case 0: Instruction = "LD (BC), A";	return 1;
						case 1: Instruction = "LD (DE), A";	return 1;
						case 2:
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Instruction = std::format("LD (#${:04X}), HL", Value);
							return 3;
						}
						case 3:
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Instruction = std::format("LD (#${:04X}), A", Value);
							return 3;
						}
					}
				}
				else
				{
					switch (OCTAL_P(Opcode))
					{
						case 0: Instruction = "LD A, (BC)";	return 1;
						case 1: Instruction = "LD A, (DE)";	return 1;
						case 2:
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Instruction = std::format("LD HL, (#${:04X})", Value);
							return 3;
						}
						case 3:
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Instruction = std::format("LD A, (#${:04X})", Value);
							return 3;
						}
					}
				}
			}
			case 3:
			{
				if (OCTAL_Q(Opcode) == 0)
				{
					Instruction = std::string("INC ") + Registers16[OCTAL_P(Opcode)];
				}
				else
				{
					Instruction = std::string("DEC ") + Registers16[OCTAL_P(Opcode)];
				}
				return 1;
			}
			case 4:
			{
				Instruction = std::string("INC ") + Registers8[OCTAL_Y(Opcode)];	return 1;
			}
			case 5:
			{
				Instruction = std::string("DEC ") + Registers8[OCTAL_Y(Opcode)];	return 1;
			}
			case 6:
			{
				const uint8_t Value = RawData[Address++];
				Instruction = std::format("LD {}, #${:02X}", Registers8[OCTAL_Y(Opcode)], Value);
				return 2;
			}
			case 7:
			{
				switch (OCTAL_Z(Opcode))
				{
				case 0: Instruction = std::string("RLCA");	return 1;
				case 1: Instruction = std::string("RRCA");	return 1;
				case 2: Instruction = std::string("RLA");	return 1;
				case 3: Instruction = std::string("RRA");	return 1;
				case 4: Instruction = std::string("DDA");	return 1;
				case 5: Instruction = std::string("CPL");	return 1;
				case 6: Instruction = std::string("SCF");	return 1;
				case 7: Instruction = std::string("CCF");	return 1;
				}
			}
			}
		}
		case 1:
		{
			if (OCTAL_Y(Opcode) == 6 && OCTAL_Z(Opcode) == 6)
			{
				Instruction = std::string("HALT");
			}
			else
			{
				Instruction = std::format("LD {}, {}", Registers8[OCTAL_Y(Opcode)], Registers8[OCTAL_Z(Opcode)]);
			}
			return 1;
		}
		case 2:
		{
			Instruction = std::format("{} {}", ArithmeticLogic[OCTAL_Y(Opcode)], Registers8[OCTAL_Z(Opcode)]);
			return 1;
		}
		case 3:
		{
			switch (OCTAL_Z(Opcode))
			{
				case 0:
					Instruction = std::format("RET !{}", Conditions[OCTAL_Y(Opcode)]);
					return 1;
				case 1:
				{
					if (OCTAL_Q(Opcode) == 0)
					{
						const uint8_t P = OCTAL_P(Opcode);
						Instruction = std::string("POP ") + Registers16[P == 3 ? P + 1 : P];
						return 1;
					}
					else
					{
						switch (OCTAL_P(Opcode))
						{
						case 0: Instruction = "RET";		return 1;
						case 1: Instruction = "EXX";		return 1;
						case 2: Instruction = "JP (HL)";	return 1;
						case 3: Instruction = "LD SP, HL";	return 1;
						}
					}
				}
				case 2:
				{
					const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
					Instruction = std::format("JP !{}, #${:04X}", Conditions[OCTAL_Y(Opcode)], Value);
					return 3;
				}
				case 3:
				{
					switch (OCTAL_Y(Opcode))
					{
						case 0:
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Instruction = std::format("JP #${:04X}", Value);
							return 3;
						}
						case 1: return 0xCB;
						case 2:
						{
							const uint8_t Value = RawData[Address++];
							Instruction = std::format("OUT (#${:02X}), A", Value);
							return 2;
						}
						case 3:
						{
							const uint8_t Value = RawData[Address++];
							Instruction = std::format("IN A, (#${:02X})", Value);
							return 2;
						}
						case 4:
							Instruction = "EX (SP), HL";
							return 1;
						case 5:
							Instruction = "EX DE, HL";
							return 1;
						case 6:
							Instruction = "DI";
							return 1;
						case 7:
							Instruction = "EI";
							return 1;
					}
				}
				case 4:
				{
					const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
					Instruction = std::format("CALL !{}, #${:04X}", Conditions[OCTAL_Y(Opcode)], Value);
					return 3;
				}
				case 5:
				{
					if (OCTAL_Q(Opcode) == 0)
					{
						const uint8_t P = OCTAL_P(Opcode);
						Instruction = std::string("PUSH ") + Registers16[P == 3 ? P + 1 : P];
						return 1;
					}
					else
					{
						switch (OCTAL_P(Opcode))
						{
						case 0:
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Instruction = std::format("CALL #${:04X}", Value);
							return 3;
						}
						case 1: return 0xDD;
						case 2: return 0xED;
						case 3: return 0xFD;
						}
					}
				}
				case 6:
				{
					const uint8_t Value = RawData[Address++];
					Instruction = std::format("{} #${:02X}", ArithmeticLogic[OCTAL_Y(Opcode)], Value);
					return 2;
				}
				case 7:
				{
					Instruction = std::format("RST #${:02X}", OCTAL_Y(Opcode) * 8);
					return 1;
				}
			}
		}
		}
		assert(false);
		return 0;
	}

	uint8_t CB_Opcodes(uint16_t& Address, const uint8_t* RawData, std::string& Instruction)
	{
		const uint8_t Opcode = RawData[Address++];
		switch (OCTAL_X(Opcode))
		{
			case 0:
			{
				Instruction = std::format("{} {}", RotationShift[OCTAL_Y(Opcode)], Registers8[OCTAL_Z(Opcode)]);
				return 2;
			}
			case 1:
			{
				Instruction = std::format("BIT {}, {}", OCTAL_Y(Opcode), Registers8[OCTAL_Z(Opcode)]);
				return 2;
			}
			case 2:
			{
				Instruction = std::format("RES {}, {}", OCTAL_Y(Opcode), Registers8[OCTAL_Z(Opcode)]);
				return 2;
			}
			case 3:
			{
				Instruction = std::format("SET {}, {}", OCTAL_Y(Opcode), Registers8[OCTAL_Z(Opcode)]);
				return 2;
			}
		}
		assert(false);
		return 0;
	}

	uint8_t Instruction(std::string& Instruction, uint16_t& Address, const uint8_t* RawData)
	{
		uint8_t Result = UnprefixedOpcodes(Address, RawData, Instruction);
		switch (Result)
		{
		case 0xCB: return CB_Opcodes(Address, RawData, Instruction);
		case 0xDD:
		case 0xED:
		case 0xFD:
		default:
			return Result;
		}
	}

	void StepLine(uint16_t& Address, int32_t Steps, const std::vector<uint8_t>& AddressSpace)
	{
		std::string Command;
		if (Steps > 0)
		{
			for (int32_t i = 0; i < Steps; ++i)
			{
				Instruction(Command, Address, AddressSpace.data());
			}
		}
		else
		{
			uint16_t TmpAddress;
			uint8_t InstructionSize, BackStep;
			for (int32_t i = Steps; i != 0; ++i)
			{
				BackStep = 5, InstructionSize = 0;
				do
				{
					TmpAddress = Address - BackStep;
					InstructionSize = Instruction(Command, TmpAddress, AddressSpace.data());
				} while (--BackStep && TmpAddress != Address);
				Address -= InstructionSize;
			}
		}
	}

	std::string Operation(std::string& String, const std::string& Delimiter = " ")
	{
		size_t Pos = 0;
		std::string Mnemonic;
		if ((Pos = String.find(Delimiter)) != std::string::npos)
		{
			Mnemonic = String.substr(0, Pos);
			String.erase(0, Pos + Delimiter.length());
		}
		else
		{
			Mnemonic = String;
			String.clear();
		}
		return Mnemonic;
	}

	std::string CutOutWordPart(std::string& String, const std::vector<std::string>& PriorityDelimiter = {" "})
	{
		size_t Pos = 0;
		for (const std::string& Delimiter : PriorityDelimiter)
		{
			while ((Pos = String.find(Delimiter)) != std::string::npos)
			{
				std::string WordPart = String.substr(0, Pos);
				String.erase(0, Pos);
				return WordPart;
			}
		}
		std::string WordPart = String;
		String.clear();
		return WordPart;
	}

	std::string SyntacticParse(std::string& String, ESyntacticType& SyntacticType)
	{
		if (String.empty())
		{
			SyntacticType = ESyntacticType::None;
			return "";
		}
		const char Symbol = String[0];
		String.erase(0, 1);

		if (Symbol >= 'A' && Symbol <= 'Z')
		{
			std::string Register = Symbol + CutOutWordPart(String, {", ", " "});
			SyntacticType = ESyntacticType::Register;
			return Register;
		}
		else if (Symbol == ',' ||
				 Symbol == '#' ||
				 Symbol == '(' ||
				 Symbol == ')' ||
				 Symbol == ' '   )
		{
			SyntacticType = ESyntacticType::Symbol;
			return std::string(1, Symbol);
		}
		else if (Symbol == '!')
		{
			std::string Value = CutOutWordPart(String, {", "});
			SyntacticType = ESyntacticType::Condition;
			return Value;
		}
		else if (Symbol == '$')
		{
			std::string Value = CutOutWordPart(String, {")"});
			SyntacticType = ESyntacticType::Constant;
			return Value;
		}
		
		return String;
	}
}

SDisassembler::SDisassembler(EFont::Type _FontName)
	: SWindow(ThisWindowName, _FontName, true)
	, bMemoryArea(false)
	, bShowStatusBar(true)
	, CodeDisassemblerScale(1.0f)
	, PrevCursorAtAddress(INDEX_NONE)
	, CursorAtAddress(INDEX_NONE)
	, LatestClockCounter(INDEX_NONE)
{}

void SDisassembler::Initialize()
{}

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
		Input_Mouse();

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
	PrevCursorAtAddress = INDEX_NONE;
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

	float FooterHeight = 0;
	const float HeightSeparator = ImGui::GetStyle().ItemSpacing.y;
	if (bShowStatusBar)
		FooterHeight += HeightSeparator + ImGui::GetFrameHeightWithSpacing() * 1;

	ImGui::BeginChild("##Scrolling", ImVec2(0, -FooterHeight), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration);
	ImGui::PushFont(FFonts::Get().GetFont(NAME_DISASSEMBLER_16));
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 5, 0 });
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

	CodeDisassemblerID = ImGui::GetCurrentWindow()->ID;

	if (ImGui::BeginTable("##Disassembler", bMemoryArea ? 4 : 3,
		ImGuiTableFlags_NoPadOuterX |
		ImGuiTableFlags_NoClip |
		ImGuiTableFlags_NoBordersInBodyUntilResize |
		ImGuiTableFlags_Resizable
	))
	{
		ImGui::TableSetupColumn("Breakpoint", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_Breakpoint * CodeDisassemblerScale);
		if (bMemoryArea) ImGui::TableSetupColumn("PrefixAddress", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_PrefixAddress * CodeDisassemblerScale);
		ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_Address * CodeDisassemblerScale);
		ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch , ColumnWidth_Instruction * CodeDisassemblerScale);

		// calculate address step
		{
			ImGuiWindow* Window = ImGui::GetCurrentWindow();

			const float MaxStep = Window->InnerRect.GetHeight() * 0.67f;
			const float ScrollStep = ImTrunc(ImMin(5 * Window->CalcFontSize(), MaxStep));
			float a = Window->CalcFontSize();

			if (Window->Scroll.y == 0 && MouseWheel > 0)
			{
				if (PrevCursorAtAddress == (uint16_t)INDEX_NONE)
				{
					Disassembler::StepLine(CursorAtAddress, -1, AddressSpace);
				}
				else
				{
					CursorAtAddress = PrevCursorAtAddress;
					PrevCursorAtAddress = INDEX_NONE;
				}
			}
			else if (Window->Scroll.y == Window->ScrollMax.y && MouseWheel < 0)
			{
				PrevCursorAtAddress = CursorAtAddress;
				Disassembler::StepLine(CursorAtAddress, 1, AddressSpace);
				Window->Scroll.y = 0;
			}
		}

		uint16_t Address = CursorAtAddress;
		const int32_t Lines = UI::GetVisibleLines(FontName) + 1;
		for (int32_t i = 0; i < Lines; ++i)
		{
			ImGui::TableNextRow();

			Draw_Breakpoint(Address);
			Draw_Address(Address);
			Draw_Instruction(Address);
		}
		ImGui::EndTable();
	}

	ImGui::PopStyleVar(2);
	ImGui::PopFont();
	ImGui::EndChild();

	ImGui::Separator();
	ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_WEAK));
	UI::TextAligned(std::format("{}%", 100.0f * CodeDisassemblerScale).c_str(), { 1.0f, 0.5f });
	ImGui::PopStyleColor();
}

void SDisassembler::Draw_Breakpoint(uint16_t Address)
{
	ImGui::TableNextColumn();
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BREAKPOINT));
	//ImGui::TextColored(COL_CONST(UI::COLOR_REGISTERS), "");
}

void SDisassembler::Draw_Address(uint16_t Address)
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

uint32_t SDisassembler::Draw_Instruction(uint16_t& Address)
{
	std::string Command;
	std::vector<std::string> Instruction;
	const uint32_t Length = Disassembler::Instruction(Command, Address, AddressSpace.data());

	ImGui::TableNextColumn();
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BG_MNEMONIC));
	static const float Offset[] = { 10, 100, 150 };
	static const ImVec4 Color[] = { COL_CONST(UI::COLOR_DISASM_INSTRUCTION), COL_CONST(UI::COLOR_DISASM_MNEMONIC), COL_CONST(UI::COLOR_DISASM_MNEMONIC) };

	ImGuiWindow* Window = ImGui::GetCurrentWindow();
	if (!Window->SkipItems)
	{
		const float FirstPadding = 10.0f;
		const float SecondPadding = FirstPadding + 80.0f;
		const ImVec2 Padding = { 0.0f, 0.0f };
		const ImVec2 Aligment = { 0.0f, 0.0f };
		const ImVec2 Position = Window->DC.CursorPos;

		ImGuiContext& Context = *GImGui;
		ImGuiStyle& Style = Context.Style;

		// draw mnemonic
		ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_DISASM_INSTRUCTION));

		std::string Operation = Disassembler::Operation(Command);

		const ImVec2 StringSize = ImGui::CalcTextSize(Operation.c_str(), nullptr, true);
		const ImVec2 Size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), StringSize.x + Padding.x * 2.0f, StringSize.y + Padding.y * 2.0f);
		const ImRect bb(Position, Position + Size);
		ImGui::RenderTextClipped(bb.Min + Padding + ImVec2(FirstPadding, 0.0f) * CodeDisassemblerScale, bb.Max - Padding, Operation.c_str(), nullptr, &StringSize, Aligment, &bb);

		ImGui::PopStyleColor();

		// draw remaining part of command
		float TextOffset = 0;
		Disassembler::ESyntacticType Type = Disassembler::ESyntacticType::NotInit;
		while (true)
		{
			std::string Part = Disassembler::SyntacticParse(Command, Type);
			if (Type == Disassembler::ESyntacticType::None)
			{
				break;
			}
			if (Part.compare(" ") == 0)
			{
				TextOffset += Style.FramePadding.x * 2.0f;
				continue;
			}

			ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST((int32_t)Type));

			const ImVec2 StringSize = ImGui::CalcTextSize(Part.c_str(), nullptr, true);
			const ImVec2 Size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), StringSize.x + Padding.x * 2.0f, StringSize.y + Padding.y * 2.0f);
			const ImRect bb(Position, Position + Size);
			ImGui::RenderTextClipped(bb.Min + Padding + ImVec2(SecondPadding * CodeDisassemblerScale + TextOffset, 0.0f), bb.Max - Padding, Part.c_str(), nullptr, &StringSize, Aligment, &bb);

			TextOffset += StringSize.x;

			ImGui::PopStyleColor();
		}
	}	

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

void SDisassembler::Input_Mouse()
{
	ImGuiContext& Context = *GImGui;
	MouseWheel = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner) ? Context.IO.MouseWheel : 0.0f;

	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != CodeDisassemblerID)
	{
		return;
	}

	if (MouseWheel != 0.0f && Context.IO.KeyCtrl && !Context.IO.FontAllowUserScaling)
	{
		CodeDisassemblerScale += MouseWheel * 0.0625f;
		CodeDisassemblerScale = FFonts::Get().SetSize(NAME_DISASSEMBLER_16, CodeDisassemblerScale, 0.5f, 1.5f);
		MouseWheel = 0.0f; // reset
	}
}
