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
	static constexpr const char* Registers8[]				= { "B", "C", "D", "E", "H", "L", "(HL)", "A" };
	static constexpr const char* Registers16[]				= { "BC", "DE", "HL", "SP", "AF" };
	static constexpr const char* Conditions[]				= { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M" };
	static constexpr const char* ArithmeticLogic[]			= { "ADD A,", "ADC A,", "SUB", "SBC A,", "AND", "XOR", "OR", "CP" };
	static constexpr const char* RotationShift[]			= { "RLC", "RRC", "RL", "RR", "SLA", "SRA", "SLL", "SRL" };
	static constexpr const char* InterruptModes[]			= { "0", "0/1", "1", "2", "0", "0/1", "1", "2" };
	static constexpr const char* BlockInstructions[4][4]	= { { "LDI", "CPI", "INI", "OUTI" },
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

	enum class EIndexRegisterType
	{
		None,
		IX,
		IY,
	};

	#define OCTAL_X(opcode)		((opcode & 0xC0) >> 6)
	#define OCTAL_Y(opcode)		((opcode & 0x38) >> 3)
	#define OCTAL_Z(opcode)		((opcode & 0x07) >> 0)
	#define OCTAL_P(opcode)		((opcode & 0x30) >> 4)
	#define OCTAL_Q(opcode)		((opcode & 0x08) >> 3)

	uint8_t UnprefixedOpcodes(uint16_t& Address, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter, EIndexRegisterType IndexRegister = EIndexRegisterType::None)
	{
		Counter++;
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
							case 0:
							{
								Instruction = "NOP";
								break;
							}
							case 1:
							{
								Instruction = "EX AF, AF'";
								break;
							}
							case 2:
							{
								const int16_t Value = (signed char)RawData[Address++];
								Instruction = std::format("DJNZ #${:04X}", uint16_t(Value + Address));
								Counter++;
								break;
							}
							case 3:
							{
								const int16_t Value = (signed char)RawData[Address++];
								Instruction = std::format("JR #${:04X}", uint16_t(Value + Address));
								Counter++;
								break;
							}
							case 4:
							case 5:
							case 6:
							case 7:
							{
								const int16_t Value = (signed char)RawData[Address++];
								Instruction = std::format("JR !{}, #${:04X}", Conditions[OCTAL_Y(Opcode) - 4], uint16_t(Value + Address));
								Counter++;
								break;
							}
						}
						break;
					}
					case 1:
					{
						if (OCTAL_Q(Opcode) == 0)
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Instruction = std::format("LD {}, #${:04X}", Registers16[OCTAL_P(Opcode)], Value);
							Counter += 2;
						}
						else
						{
							Instruction = std::string("ADD HL, ") + Registers16[OCTAL_P(Opcode)];
						}
						break;
					}
					case 2:
					{
						if (OCTAL_Q(Opcode) == 0)
						{
							switch (OCTAL_P(Opcode))
							{
								case 0:
								{
									Instruction = "LD (BC), A";
									break;
								}
								case 1:
								{
									Instruction = "LD (DE), A";
									break;
								}
								case 2:
								{
									const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
									Instruction = std::format("LD (#${:04X}), HL", Value);
									Counter += 2;
									break;
								}
								case 3:
								{
									const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
									Instruction = std::format("LD (#${:04X}), A", Value);
									Counter += 2;
									break;
								}
							}
						}
						else
						{
							switch (OCTAL_P(Opcode))
							{
								case 0:
								{
									Instruction = "LD A, (BC)";
									break;
								}
								case 1:
								{
									Instruction = "LD A, (DE)";
									break;
								}
								case 2:
								{
									const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
									Instruction = std::format("LD HL, (#${:04X})", Value);
									Counter += 2;
									break;
								}
								case 3:
								{
									const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
									Instruction = std::format("LD A, (#${:04X})", Value);
									Counter += 2;
									break;
								}
							}
						}
						break;
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
						break;
					}
					case 4:
					{
						Instruction = std::string("INC ") + Registers8[OCTAL_Y(Opcode)];
						break;
					}
					case 5:
					{
						Instruction = std::string("DEC ") + Registers8[OCTAL_Y(Opcode)];
						break;
					}
					case 6:
					{
						const uint8_t Value = RawData[Address++];
						Instruction = std::format("LD {}, #${:02X}", Registers8[OCTAL_Y(Opcode)], Value);
						Counter++;
						break;
					}
					case 7:
					{
						switch (OCTAL_Z(Opcode))
						{
							case 0: Instruction = "RLCA";	break;
							case 1: Instruction = "RRCA";	break;
							case 2: Instruction = "RLA";	break;
							case 3: Instruction = "RRA";	break;
							case 4: Instruction = "DDA";	break;
							case 5: Instruction = "CPL";	break;
							case 6: Instruction = "SCF";	break;
							case 7: Instruction = "CCF";	break;
						}
						break;
					}
				}
				break;
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
				break;
			}
			case 2:
			{
				Instruction = std::format("{} {}", ArithmeticLogic[OCTAL_Y(Opcode)], Registers8[OCTAL_Z(Opcode)]);
				break;
			}
			case 3:
			{
				switch (OCTAL_Z(Opcode))
				{
					case 0:
					{
						Instruction = std::format("RET !{}", Conditions[OCTAL_Y(Opcode)]);
						break;
					}
					case 1:
					{
						if (OCTAL_Q(Opcode) == 0)
						{
							const uint8_t P = OCTAL_P(Opcode);
							Instruction = std::string("POP ") + Registers16[P == 3 ? P + 1 : P];
						}
						else
						{
							switch (OCTAL_P(Opcode))
							{
							case 0: Instruction = "RET";		break;
							case 1: Instruction = "EXX";		break;
							case 2: Instruction = "JP (HL)";	break;
							case 3: Instruction = "LD SP, HL";	break;
							}
						}
						break;
					}
					case 2:
					{
						const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
						Instruction = std::format("JP !{}, #${:04X}", Conditions[OCTAL_Y(Opcode)], Value);
						Counter +=2;
						break;
					}
					case 3:
					{
						switch (OCTAL_Y(Opcode))
						{
							case 0:
							{
								const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
								Instruction = std::format("JP #${:04X}", Value);
								Counter += 2;
								break;
							}
							case 1: return 0xCB;
							case 2:
							{
								const uint8_t Value = RawData[Address++];
								Instruction = std::format("OUT (#${:02X}), A", Value);
								Counter++;
								break;
							}
							case 3:
							{
								const uint8_t Value = RawData[Address++];
								Instruction = std::format("IN A, (#${:02X})", Value);
								Counter++;
								break;
							}
							case 4:
							{
								Instruction = "EX (SP), HL";
								break;
							}
							case 5:
							{
								Instruction = "EX DE, HL";
								break;
							}
							case 6:
							{
								Instruction = "DI";
								break;
							}
							case 7:
							{
								Instruction = "EI";
								break;
							}
						}
						break;
					}
					case 4:
					{
						const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
						Instruction = std::format("CALL !{}, #${:04X}", Conditions[OCTAL_Y(Opcode)], Value);
						Counter += 2;
						break;
					}
					case 5:
					{
						if (OCTAL_Q(Opcode) == 0)
						{
							const uint8_t P = OCTAL_P(Opcode);
							Instruction = std::string("PUSH ") + Registers16[P == 3 ? P + 1 : P];
						}
						else
						{
							switch (OCTAL_P(Opcode))
							{
								case 0:
								{
									const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
									Instruction = std::format("CALL #${:04X}", Value);
									Counter += 2;
									break;
								}
								case 1: return 0xDD;
								case 2: return 0xED;
								case 3: return 0xFD;
							}
						}
						break;
					}
					case 6:
					{
						const uint8_t Value = RawData[Address++];
						Instruction = std::format("{} #${:02X}", ArithmeticLogic[OCTAL_Y(Opcode)], Value);
						Counter++;
						break;
					}
					case 7:
					{
						Instruction = std::format("RST #${:02X}", OCTAL_Y(Opcode) * 8);
						break;
					}
				}
				break;
			}
		}
		return 0;
	}

	uint8_t CB_Opcodes(uint16_t& Address, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter)
	{
		Counter++;
		const uint8_t Opcode = RawData[Address++];
		switch (OCTAL_X(Opcode))
		{
			case 0:
			{
				Instruction = std::format("{} {}", RotationShift[OCTAL_Y(Opcode)], Registers8[OCTAL_Z(Opcode)]);
				break;
			}
			case 1:
			{
				Instruction = std::format("BIT {}, {}", OCTAL_Y(Opcode), Registers8[OCTAL_Z(Opcode)]);
				break;
			}
			case 2:
			{
				Instruction = std::format("RES {}, {}", OCTAL_Y(Opcode), Registers8[OCTAL_Z(Opcode)]);
				break;
			}
			case 3:
			{
				Instruction = std::format("SET {}, {}", OCTAL_Y(Opcode), Registers8[OCTAL_Z(Opcode)]);
				break;
			}
		}
		return 0;
	}

	uint8_t ED_Opcodes(uint16_t& Address, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter);
	uint8_t FD_Opcodes(uint16_t& Address, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter);
	uint8_t DD_Opcodes(uint16_t& Address, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter)
	{
		Counter++;
		const uint8_t Opcode = RawData[Address++];
		switch (Opcode)
		{
			case 0xDD: DD_Opcodes(Address, RawData, Instruction, Counter);									break;
			case 0xED: ED_Opcodes(Address, RawData, Instruction, Counter);									break;
			case 0xFD: FD_Opcodes(Address, RawData, Instruction, Counter);									break;
			default:   UnprefixedOpcodes(Address, RawData, Instruction, Counter, EIndexRegisterType::IX);	break;
		}
		return Counter;
	}

	uint8_t ED_Opcodes(uint16_t& Address, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter)
	{
		bool bNMOS = true;
		Counter++;
		const uint8_t Opcode = RawData[Address++];
		switch (OCTAL_X(Opcode))
		{
			case 0:
			case 3:
			{
				Instruction = "NOP*";
				break;
			}
			case 1:
			{
				switch (OCTAL_Z(Opcode))
				{
					case 0:
					{
						if (OCTAL_Y(Opcode) == 6)
						{
							Instruction = "IN F, (C)";
							break;
						}
						else
						{
							Instruction = std::format("IN {}, (C)",  Registers8[OCTAL_Y(Opcode)]);
							break;
						}
						break;
					}
					case 1:
					{
						if (OCTAL_Y(Opcode) == 6)
						{
							Instruction = std::string("OUT (C), #") + (bNMOS ? "00" : "FF");
							break;
						}
						else
						{
							Instruction = std::format("OUT (C), {}", Registers8[OCTAL_Y(Opcode)]);
							break;
						}
						break;
					}
					case 2:
					{
						if (OCTAL_Q(Opcode) == 0)
						{
							Instruction = std::string("SBC HL, ") + Registers16[OCTAL_P(Opcode)];
						}
						else
						{
							Instruction = std::string("ADC HL, ") + Registers16[OCTAL_P(Opcode)];
						}
						break;
					}
					case 3:
					{
						if (OCTAL_Q(Opcode) == 0)
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Instruction = std::format("LD (#${:04X}), {}", Value, Registers16[OCTAL_P(Opcode)]);
							Counter += 2;
						}
						else
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Instruction = std::format("LD {}, (#${:04X})", Registers16[OCTAL_P(Opcode)], Value);
							Counter += 2;
						}
						break;
					}
					case 4:
					{
						Instruction = "NEG";
						break;
					}
					case 5:
					{
						if (OCTAL_Y(Opcode) == 1)
						{
							Instruction = "RETI";
						}
						else
						{
							Instruction = "RETN";
						}
						break;
					}
					case 6:
					{
						Instruction = std::format("IM {}", InterruptModes[OCTAL_Y(Opcode)]);
						break;
					}
					case 7:
					{
						switch (OCTAL_Y(Opcode))
						{
							case 0: Instruction = "LD I, A";	break;
							case 1: Instruction = "LD R, A";	break;
							case 2: Instruction = "LD A, I";	break;
							case 3: Instruction = "LD A, R";	break;
							case 4: Instruction = "RRD";		break;
							case 5: Instruction = "RLD";		break;
							case 6: Instruction = "NOP";		break;
							case 7: Instruction = "NOP";		break;
						}
						break;
					}
				}
				break;
			}
			case 2:
			{
				auto z = OCTAL_Z(Opcode);
				auto y = OCTAL_Y(Opcode);
				if ((OCTAL_Z(Opcode) <= 3) && (OCTAL_Y(Opcode) >= 4))
				{
					Instruction = BlockInstructions[OCTAL_Y(Opcode) - 4][OCTAL_Z(Opcode)];
				}
				else
				{
					Instruction = "NOP*";
				}
				break;
			}
		}
		return 0;
	}

	uint8_t FD_Opcodes(uint16_t& Address, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter)
	{
		Counter++;
		const uint8_t Opcode = RawData[Address++];
		switch (Opcode)
		{
		case 0xDD: DD_Opcodes(Address, RawData, Instruction, Counter);									break;
		case 0xED: ED_Opcodes(Address, RawData, Instruction, Counter);									break;
		case 0xFD: FD_Opcodes(Address, RawData, Instruction, Counter);									break;
		default:   UnprefixedOpcodes(Address, RawData, Instruction, Counter, EIndexRegisterType::IY);	break;
		}
		return Counter;
	}

	uint8_t Instruction(std::string& Instruction, uint16_t& Address, const uint8_t* RawData)
	{
		uint8_t Counter = 0;
		switch (UnprefixedOpcodes(Address, RawData, Instruction, Counter))
		{
		case 0xCB: CB_Opcodes(Address, RawData, Instruction, Counter); break;
		case 0xDD: DD_Opcodes(Address, RawData, Instruction, Counter); break;
		case 0xED: ED_Opcodes(Address, RawData, Instruction, Counter); break;
		case 0xFD: FD_Opcodes(Address, RawData, Instruction, Counter); break;
		}
		return Counter;
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
				BackStep = 16, InstructionSize = 0;
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
				 Symbol == ' ' ||
				 Symbol == '+'   )
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
		else if (Symbol >= '0' && Symbol <= '8')
		{
			SyntacticType = ESyntacticType::Constant;
			return std::string(1, Symbol);
		}
		else if (Symbol == '^')
		{
			SyntacticType = ESyntacticType::ConstantOffset;
			std::string Value = CutOutWordPart(String, { ")" });
			SyntacticType = ESyntacticType::Constant;
			return Value;
		}
		else
		{

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

		const int32_t Lines = UI::GetVisibleLines(FontName);
		
		// handler input steps up/down
		{
			ImGuiWindow* Window = ImGui::GetCurrentWindow();

			switch (ActionInput)
			{
				case EDisassemblerInput::MouseWheelUp:
				{
					Disassembler::StepLine(CursorAtAddress, -1, AddressSpace);
					break;
				}
				case EDisassemblerInput::MouseWheelDown:
				{
					Disassembler::StepLine(CursorAtAddress, 1, AddressSpace);
					Window->Scroll.y = 0;
					break;
				}
				case EDisassemblerInput::PageUpArrow:
				{
					Disassembler::StepLine(CursorAtAddress, -1, AddressSpace);
					break;
				}
				case EDisassemblerInput::PageDownArrow:
				{
					Disassembler::StepLine(CursorAtAddress, 1, AddressSpace);
					break;
				}
				case EDisassemblerInput::PageUpPressed:
				{
					Disassembler::StepLine(CursorAtAddress, -Lines, AddressSpace);
					break;
				}
				case EDisassemblerInput::PageDownPressed:
				{
					Disassembler::StepLine(CursorAtAddress, Lines, AddressSpace);
					break;
				}
			}
			ActionInput = EDisassemblerInput::None;
		}

		uint16_t Address = CursorAtAddress;
		for (int32_t i = 0; i < Lines + 1; ++i)
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
		{ ImGuiKey_UpArrow,		ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_UpArrow, this)					},	// debugger: up arrow
		{ ImGuiKey_DownArrow,	ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_DownArrow, this)				},	// debugger: down arrow
		{ ImGuiKey_PageUp,		ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_PageUp, this)					},	// debugger: page up
		{ ImGuiKey_PageDown,	ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_PageDown, this)					},	// debugger: page down

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
	ActionInput = MouseWheel == 0 ? ActionInput :
				  MouseWheel > 0  ? EDisassemblerInput::MouseWheelUp : EDisassemblerInput::MouseWheelDown;

	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != CodeDisassemblerID)
	{
		return;
	}

	if (MouseWheel != 0.0f && Context.IO.KeyCtrl && !Context.IO.FontAllowUserScaling)
	{
		CodeDisassemblerScale += MouseWheel * 0.0625f;
		CodeDisassemblerScale = FFonts::Get().SetSize(NAME_DISASSEMBLER_16, CodeDisassemblerScale, 0.5f, 1.5f);
		ActionInput = EDisassemblerInput::None;
		MouseWheel = 0.0f; // reset
	}
}

void SDisassembler::Input_UpArrow()
{
	ActionInput = EDisassemblerInput::PageUpArrow;
}

void SDisassembler::Input_DownArrow()
{
	ActionInput = EDisassemblerInput::PageDownArrow;
}

void SDisassembler::Input_PageUp()
{
	ActionInput = EDisassemblerInput::PageUpPressed;
}

void SDisassembler::Input_PageDown()
{
	ActionInput = EDisassemblerInput::PageDownPressed;
}
