#include "Disassembler.h"

#include "AppDebugger.h"
#include "Utils/UI.h"
#include "Utils/Hotkey.h"
#include "Utils/Memory.h"
#include "Devices/CPU/Z80.h"
#include "CPU_State.h"
#include "Motherboard/Motherboard.h"
#include "resource.h"

namespace
{
	static const char* ThisWindowName = TEXT("Disassembler");

	// set column widths
	static constexpr float ColumnWidth_Breakpoint = 2;
	static constexpr float ColumnWidth_PrefixAddress = 10.0f;
	static constexpr float ColumnWidth_Address = 50.0f;
	static constexpr float ColumnWidth_Opcode = 80.0f;
	static constexpr float ColumnWidth_Instruction = 200.0f;

	#define FORMAT_ADDRESS(upper)		(upper ? "%04X" : "%04x")
	#define FORMAT_OPCODE(upper)		(upper ? "{:02X}" : "{:02x}")
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

	#define LOW(value)			((value >> 0) & 0xFF)
	#define HIGH(value)			((value >> 8) & 0xFF)

	uint8_t UnprefixedOpcodes(uint16_t& Address, std::string& Opcodes, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter, EIndexRegisterType& IndexRegister)
	{
		const uint8_t Opcode = RawData[Address++];
		if (IndexRegister == EIndexRegisterType::None)
		{
			Opcodes += std::format(FORMAT_OPCODE(true), Opcode);
			Counter++;
		}

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
								Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
								Instruction = std::format("DJNZ #${:04X}", uint16_t(Value + Address));
								Counter++;
								break;
							}
							case 3:
							{
								const int16_t Value = (signed char)RawData[Address++];
								Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
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
								Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
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
							Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
							Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
							if (IndexRegister != EIndexRegisterType::None && OCTAL_P(Opcode) == 2 /*HL*/)
							{
								const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
								Instruction = std::format("LD {}, #${:04X}", IndexRegisterPair, Value);
							}
							else
							{
								Instruction = std::format("LD {}, #${:04X}", Registers16[OCTAL_P(Opcode)], Value);
							}
							Counter += 2;
						}
						else
						{
							if (IndexRegister != EIndexRegisterType::None)
							{
								const std::string IndexRegisterPairA = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
								const std::string IndexRegisterPairB = OCTAL_P(Opcode) == 2 /*HL*/ ? IndexRegisterPairA : Registers16[OCTAL_P(Opcode)];
								Instruction = std::format("ADD {}, {}", IndexRegisterPairA, IndexRegisterPairB);
							}
							else
							{
								Instruction = std::string("ADD HL, ") + Registers16[OCTAL_P(Opcode)];
							}
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
									Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
									Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
									if (IndexRegister != EIndexRegisterType::None && OCTAL_P(Opcode) == 2 /*HL*/)
									{
										const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
										Instruction = std::format("LD (#${:04X}), {}", Value, IndexRegisterPair);
									}
									else
									{
										Instruction = std::format("LD (#${:04X}), HL", Value);
									}
									Counter += 2;
									break;
								}
								case 3:
								{
									const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
									Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
									Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
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
									Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
									Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
									if (IndexRegister != EIndexRegisterType::None && OCTAL_P(Opcode) == 2 /*HL*/)
									{
										const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
										Instruction = std::format("LD {}, (#${:04X})", IndexRegisterPair, Value);
									}
									else
									{
										Instruction = std::format("LD HL, (#${:04X})", Value);
									}
									Counter += 2;
									break;
								}
								case 3:
								{
									const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
									Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
									Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
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
							if (IndexRegister != EIndexRegisterType::None && OCTAL_P(Opcode) == 2 /*HL*/)
							{
								const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
								Instruction = std::string("INC ") + IndexRegisterPair;
							}
							else
							{
								Instruction = std::string("INC ") + Registers16[OCTAL_P(Opcode)];
							}
						}
						else
						{
							if (IndexRegister != EIndexRegisterType::None && OCTAL_P(Opcode) == 2 /*HL*/)
							{
								const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
								Instruction = std::string("DEC ") + IndexRegisterPair;
							}
							else
							{
								Instruction = std::string("DEC ") + Registers16[OCTAL_P(Opcode)];
							}
						}
						break;
					}
					case 4:
					{
						const uint8_t Y = OCTAL_Y(Opcode);
						if (IndexRegister != EIndexRegisterType::None && Y >= 4 /*H, L, (HL)*/ && Y <= 6)
						{
							const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
							if (Y != 6)
							{
								Instruction = std::string("INC ") + IndexRegisterPair + Registers8[Y];
							}
							else // (HL)
							{
								const uint8_t Offset = RawData[Address++];
								Opcodes += std::format(FORMAT_OPCODE(true), Offset);
								Instruction = std::format("INC ({}+#^{:02X})", IndexRegisterPair, Offset);
								Counter++;
							}
						}
						else
						{
							Instruction = std::string("INC ") + Registers8[Y];
						}
						break;
					}
					case 5:
					{
						const uint8_t Y = OCTAL_Y(Opcode);
						if (IndexRegister != EIndexRegisterType::None && Y >= 4 /*H, L, (HL)*/ && Y <= 6)
						{
							const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
							if (Y != 6)
							{
								Instruction = std::string("DEC ") + IndexRegisterPair + Registers8[Y];
							}
							else // (HL)
							{
								const uint8_t Offset = RawData[Address++];
								Opcodes += std::format(FORMAT_OPCODE(true), Offset);
								Instruction = std::format("DEC ({}+#^{:02X})", IndexRegisterPair, Offset);
								Counter++;
							}
						}
						else
						{
							Instruction = std::string("DEC ") + Registers8[Y];
						}
						break;
					}
					case 6:
					{
						const uint8_t Y = OCTAL_Y(Opcode);
						const uint8_t Value = RawData[Address++];
						Opcodes += std::format(FORMAT_OPCODE(true), Value);
						Counter++;
						if (IndexRegister != EIndexRegisterType::None && Y >= 4 /*H, L, (HL)*/ && Y <= 6)
						{
							const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
							if (Y != 6)
							{
								Instruction = std::format("LD {}{}, #${:02X}", IndexRegisterPair, Registers8[Y], Value);
							}
							else // (HL)
							{
								const uint8_t Offset = RawData[Address++];
								Opcodes += std::format(FORMAT_OPCODE(true), Offset);
								Counter++;
								Instruction = std::format("LD ({}+#^{:02X}), #${:02X}", IndexRegisterPair, Offset, Value);
							}
						}
						else
						{
							Instruction = std::format("LD {}, #${:02X}", Registers8[Y], Value);
						}
						break;
					}
					case 7:
					{
						switch (OCTAL_Y(Opcode))
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
				const uint8_t Y = OCTAL_Y(Opcode);
				const uint8_t Z = OCTAL_Z(Opcode);
				if (Y == 6 && Z == 6)
				{
					Instruction = std::string("HALT");
				}
				else
				{
					if (IndexRegister != EIndexRegisterType::None && Y >= 4 /*H, L, (HL)*/ && Y <= 6)
					{
						const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
						if (Y != 6)
						{
							Instruction = std::format("LD {}{}, {}{}", IndexRegisterPair, Registers8[Y], IndexRegisterPair, Registers8[Z]);
						}
						else // (HL)
						{
							const uint8_t Offset = RawData[Address++];
							Opcodes += std::format(FORMAT_OPCODE(true), Offset);
							Counter++;

							const std::string StringOffset = std::format("({}+#^{:02X})", IndexRegisterPair, Offset);
							const char* Lhs = Y == 6 ? StringOffset.c_str() : Registers8[Y];
							const char* Rhs = Z == 6 ? StringOffset.c_str() : Registers8[Z];
							Instruction = std::format("LD {}, {}", Lhs, Rhs);
						}
					}
					else
					{
						Instruction = std::format("LD {}, {}", Registers8[Y], Registers8[Z]);
					}
				}
				break;
			}
			case 2:
			{
				const uint8_t Y = OCTAL_Y(Opcode);
				const uint8_t Z = OCTAL_Z(Opcode);
				if (IndexRegister != EIndexRegisterType::None && Y >= 4 /*H, L, (HL)*/ && Y <= 6)
				{
					const uint8_t Value = RawData[Address++];
					Opcodes += std::format(FORMAT_OPCODE(true), Value);
					Counter++;

					const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
					if (Y != 6)
					{
						Instruction = std::format("{} {}{}", ArithmeticLogic[OCTAL_Y(Opcode)], IndexRegisterPair, Registers8[Z]);
					}
					else // (HL)
					{
						const uint8_t Offset = RawData[Address++];
						Opcodes += std::format(FORMAT_OPCODE(true), Offset);
						Counter++;
						Instruction = std::format("{} ({}+#^{:02X})", ArithmeticLogic[OCTAL_Y(Opcode)], IndexRegisterPair, Offset);
					}
				}
				else
				{
					Instruction = std::format("{} {}", ArithmeticLogic[OCTAL_Y(Opcode)], Registers8[Z]);
				}
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
							if (IndexRegister != EIndexRegisterType::None && P == 2 /*HL*/)
							{
								const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
								Instruction = std::string("POP ") + IndexRegisterPair;
							}
							else
							{
								Instruction = std::string("POP ") + Registers16[P == 3 ? P + 1 : P];
							}
						}
						else
						{
							switch (OCTAL_P(Opcode))
							{
								case 0:
								{
									Instruction = "RET";
									break;
								}
								case 1:
								{
									Instruction = "EXX";
									break;
								}
								case 2:
								{
									if (IndexRegister != EIndexRegisterType::None)
									{
										const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
										Instruction = std::format("JP ({})", IndexRegisterPair);
									}
									else
									{
										Instruction = "JP (HL)";
									}
									break;
								}
								case 3:
								{
									if (IndexRegister != EIndexRegisterType::None)
									{
										const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
										Instruction = std::format("LD SP, {}", IndexRegisterPair);
									}
									else
									{
										Instruction = "LD SP, HL";
									}
									break;
								}
							}
						}
						break;
					}
					case 2:
					{
						const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
						Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
						Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
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
								Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
								Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
								Instruction = std::format("JP #${:04X}", Value);
								Counter += 2;
								break;
							}
							case 1: return 0xCB;
							case 2:
							{
								const uint8_t Value = RawData[Address++];
								Opcodes += std::format(FORMAT_OPCODE(true), Value);
								Instruction = std::format("OUT (#${:02X}), A", Value);
								Counter++;
								break;
							}
							case 3:
							{
								const uint8_t Value = RawData[Address++];
								Opcodes += std::format(FORMAT_OPCODE(true), Value);
								Instruction = std::format("IN A, (#${:02X})", Value);
								Counter++;
								break;
							}
							case 4:
							{
								if (IndexRegister != EIndexRegisterType::None)
								{
									const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
									Instruction = std::format("EX (SP), {}", IndexRegisterPair);
								}
								else
								{
									Instruction = "EX (SP), HL";
								}
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
						Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
						Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
						Instruction = std::format("CALL !{}, #${:04X}", Conditions[OCTAL_Y(Opcode)], Value);
						Counter += 2;
						break;
					}
					case 5:
					{
						if (OCTAL_Q(Opcode) == 0)
						{
							const uint8_t P = OCTAL_P(Opcode);
							if (IndexRegister != EIndexRegisterType::None && P == 2 /*HL*/)
							{
								const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
								Instruction = std::string("PUSH ") + IndexRegisterPair;
							}
							else
							{
								Instruction = std::string("PUSH ") + Registers16[P == 3 ? P + 1 : P];
							}
						}
						else
						{
							switch (OCTAL_P(Opcode))
							{
								case 0:
								{
									const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
									Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
									Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
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
						Opcodes += std::format(FORMAT_OPCODE(true), Value);
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

	uint8_t CB_Opcodes(uint16_t& Address, std::string& Opcodes, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter, EIndexRegisterType& IndexRegister)
	{
		uint8_t Offset;
		if (IndexRegister != EIndexRegisterType::None)
		{
			Offset = RawData[Address++];
			Opcodes += std::format(FORMAT_OPCODE(true), Offset);
			Counter++;
		}
		const uint8_t Opcode = RawData[Address++];
		Opcodes += std::format(FORMAT_OPCODE(true), Opcode);
		Counter++;
		switch (OCTAL_X(Opcode))
		{
			case 0:
			{
				const uint8_t Y = OCTAL_Y(Opcode);
				const uint8_t Z = OCTAL_Z(Opcode);
				if (IndexRegister != EIndexRegisterType::None)
				{
					const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
					if (Y != 6)
					{
						Instruction = std::format("{} ({}+#^{:02X}), {}", RotationShift[Y], IndexRegisterPair, Offset, Registers8[Z]);
					}
					else // (HL)
					{
						Instruction = std::format("{} ({}+#^{:02X})", RotationShift[Y], IndexRegisterPair, Offset);
					}
				}
				else
				{
					Instruction = std::format("{} {}", RotationShift[Y], Registers8[Z]);
				}
				break;
			}
			case 1:
			{
				const uint8_t Y = OCTAL_Y(Opcode);
				const uint8_t Z = OCTAL_Z(Opcode);
				if (IndexRegister != EIndexRegisterType::None)
				{
					const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
					Instruction = std::format("BIT {}, ({}+#^{:02X})", Y, IndexRegisterPair, Offset);
				}
				else
				{
					Instruction = std::format("BIT {}, {}", Y, Registers8[Z]);
				}
				break;
			}
			case 2:
			{
				const uint8_t Y = OCTAL_Y(Opcode);
				const uint8_t Z = OCTAL_Z(Opcode);
				if (IndexRegister != EIndexRegisterType::None)
				{
					const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
					if (Y != 6)
					{
						Instruction = std::format("RES {}, ({}+#^{:02X}), {}", Y, IndexRegisterPair, Offset, Registers8[Z]);
					}
					else // (HL)
					{
						Instruction = std::format("RES {}, ({}+#^{:02X})", Y, IndexRegisterPair, Offset);
					}
				}
				else
				{
					Instruction = std::format("RES {}, {}", Y, Registers8[Z]);
				}
				break;
			}
			case 3:
			{
				const uint8_t Y = OCTAL_Y(Opcode);
				const uint8_t Z = OCTAL_Z(Opcode);
				if (IndexRegister != EIndexRegisterType::None)
				{
					const std::string IndexRegisterPair = IndexRegister == EIndexRegisterType::IX ? "IX" : "IY";
					if (Y != 6)
					{
						Instruction = std::format("SET {}, ({}+#^{:02X}), {}", Y, IndexRegisterPair, Offset, Registers8[Z]);
					}
					else // (HL)
					{
						Instruction = std::format("RES {}, ({}+#^{:02X})", Y, IndexRegisterPair, Offset);
					}
				}
				else
				{
					Instruction = std::format("RES {}, {}", Y, Registers8[Z]);
				}
				break;
			}
		}
		return 0;
	}

	uint8_t ED_Opcodes(uint16_t& Address, std::string& Opcodes, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter);
	uint8_t FD_Opcodes(uint16_t& Address, std::string& Opcodes, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter, EIndexRegisterType& IndexRegister);
	uint8_t DD_Opcodes(uint16_t& Address, std::string& Opcodes, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter, EIndexRegisterType& IndexRegister)
	{
		Counter++;
		const uint8_t Opcode = RawData[Address++];
		Opcodes += std::format(FORMAT_OPCODE(true), Opcode);
		switch (Opcode)
		{
			case 0xDD: return DD_Opcodes(Address, Opcodes, RawData, Instruction, Counter, IndexRegister);	break;
			case 0xED:		  ED_Opcodes(Address, Opcodes, RawData, Instruction, Counter);					return 0;
			case 0xFD: return FD_Opcodes(Address, Opcodes, RawData, Instruction, Counter, IndexRegister);	break;
			default:
			{
				IndexRegister = EIndexRegisterType::IX;
				return UnprefixedOpcodes(--Address, Opcodes, RawData, Instruction, Counter, IndexRegister);
			}
		}
	}

	uint8_t ED_Opcodes(uint16_t& Address, std::string& Opcodes, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter)
	{
		bool bNMOS = true;
		Counter++;
		const uint8_t Opcode = RawData[Address++];
		Opcodes += std::format(FORMAT_OPCODE(true), Opcode);
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
							Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
							Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
							Instruction = std::format("LD (#${:04X}), {}", Value, Registers16[OCTAL_P(Opcode)]);
							Counter += 2;
						}
						else
						{
							const uint16_t Value = *reinterpret_cast<const uint16_t*>(RawData + Address); Address += 2;
							Opcodes += std::format(FORMAT_OPCODE(true), LOW(Value));
							Opcodes += std::format(FORMAT_OPCODE(true), HIGH(Value));
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
				const uint8_t z = OCTAL_Z(Opcode);
				const uint8_t y = OCTAL_Y(Opcode);
				if ((z <= 3) && (y >= 4))
				{
					Instruction = BlockInstructions[y - 4][z];
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

	uint8_t FD_Opcodes(uint16_t& Address, std::string& Opcodes, const uint8_t* RawData, std::string& Instruction, uint8_t& Counter, EIndexRegisterType& IndexRegister)
	{
		Counter++;
		const uint8_t Opcode = RawData[Address++];
		Opcodes += std::format(FORMAT_OPCODE(true), Opcode);
		switch (Opcode)
		{
			case 0xDD: return DD_Opcodes(Address, Opcodes, RawData, Instruction, Counter, IndexRegister);	break;
			case 0xED:		  ED_Opcodes(Address, Opcodes, RawData, Instruction, Counter);					return 0;
			case 0xFD: return FD_Opcodes(Address, Opcodes, RawData, Instruction, Counter, IndexRegister);	break;
			default:
			{
				IndexRegister = EIndexRegisterType::IY;
				return UnprefixedOpcodes(--Address, Opcodes, RawData, Instruction, Counter, IndexRegister);
			}
		}
	}

	uint8_t Instruction(std::string& Instruction, std::string& Opcodes, uint16_t& Address, const uint8_t* RawData)
	{
		uint8_t Counter = 0;
		Opcodes.clear();
		EIndexRegisterType IndexRegister = EIndexRegisterType::None;

		uint8_t Opcode = UnprefixedOpcodes(Address, Opcodes, RawData, Instruction, Counter, IndexRegister);
		do
		{
			switch (Opcode)
			{
				case 0xCB: Opcode = CB_Opcodes(Address, Opcodes, RawData, Instruction, Counter, IndexRegister);	break;
				case 0xDD: Opcode = DD_Opcodes(Address, Opcodes, RawData, Instruction, Counter, IndexRegister);	break;
				case 0xED: Opcode = ED_Opcodes(Address, Opcodes, RawData, Instruction, Counter);				break;
				case 0xFD: Opcode = FD_Opcodes(Address, Opcodes, RawData, Instruction, Counter, IndexRegister);	break;
			}
		} while (Opcode != 0);
		return Counter;
	}

	void StepLine(uint16_t& Address, int32_t Steps, const std::vector<uint8_t>& AddressSpace)
	{
		std::string Command, Opcodes;
		if (Steps > 0)
		{
			for (int32_t i = 0; i < Steps; ++i)
			{
				Instruction(Command, Opcodes, Address, AddressSpace.data());
			}
		}
		else
		{
			//uint16_t BackStep;
			//for (int32_t s = Steps; s != 0; ++s)
			//{
			//	for (BackStep = 16; BackStep > 0; --BackStep)
			//	{
			//		uint16_t TmpAddress = Address - BackStep;
			//		if (Instruction(Command, Opcodes, TmpAddress, AddressSpace.data()) == BackStep)
			//		{
			//			Address = Address - BackStep;
			//			break;
			//		}
			//	}
			//	if (BackStep == 0)
			//	{
			//		Address--;
			//	}
			//}

			std::unordered_map<uint16_t, uint16_t> InstructionCounters;
			for (int32_t s = Steps; s != 0; ++s)
			{
				uint16_t TryCount = 16, InstructionLength;
				do
				{
					int16_t CurrentOffset = TryCount;
					do
					{
						uint16_t TmpAddress = Address - CurrentOffset;
						InstructionLength = Instruction(Command, Opcodes, TmpAddress, AddressSpace.data());
						CurrentOffset -= InstructionLength;

						if (TmpAddress == Address)
						{
							InstructionCounters[InstructionLength]++;
						}
					} while (CurrentOffset > 0);

				} while (--TryCount);

				uint16_t BestLength = 0, BestCount = 0;
				for (auto& [Length, Count] : InstructionCounters)
				{
					if (Count > BestCount)
					{
						BestCount = Count;
						BestLength = Length;
					}
				}

				if (BestLength == 0)
				{
					BestLength++;
				}

				Address -= BestLength;
				InstructionCounters.clear();
			}
		}
	}

	uint16_t GetAddressToLine(uint16_t Address, int32_t Steps, const std::vector<uint8_t>& AddressSpace)
	{
		StepLine(Address, Steps, AddressSpace);
		return Address;
	}

	bool IsAddressInArea(uint16_t TargetAddress, uint16_t TopAddress, const std::vector<uint8_t>& AddressSpace, int32_t MaxLines)
	{
		if (TargetAddress == TopAddress)
		{
			return true;
		}

		for (int32_t i = 0; i < MaxLines; ++i)
		{
			StepLine(TopAddress, 1, AddressSpace);
			if (TargetAddress == TopAddress)
			{
				return true;
			}
		}
		return false;
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
			std::string Register = Symbol + CutOutWordPart(String, {"+", " ", ", "});
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
			return Value;
		}
		else
		{

		}
		
		return String;
	}
}

SDisassembler::SDisassembler(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
	, bMemoryArea(false)
	, bShowStatusBar(true)
	, bShowOpcode(true)
	, bAddressUpperCaseHex(true)
	, bInstructionUpperCaseHex(false)
	, CodeDisassemblerScale(1.0f)
	, bEditingTakeFocusReset(false)
	, bAddressEditingTakeFocus(false)
	, bOpcodeInstructionEditingTakeFocus(false)
	, bInstructionEditingTakeFocus(false)
	, AddressEditing(INDEX_NONE)
	, OpcodeInstructionEditing(INDEX_NONE)
	, InstructionEditing(INDEX_NONE)
	, TopCursorAtAddress(INDEX_NONE)
	, UserCursorAtAddress(INDEX_NONE)
	, UserCursorAtLine(INDEX_NONE)
	, UserCursorAtColumn(DisassemblerColumn::None)
	, TimeElapsedCounter(INDEX_NONE)
	, LatestClockCounter(INDEX_NONE)
	, Status(EThreadStatus::Unknown)
{}

void SDisassembler::Initialize()
{
	ImageProgramCounter = FImageBase::LoadImageFromResource(IDB_ARROW_RIGHT, TEXT("PNG"));
}

void SDisassembler::Tick(float DeltaTime)
{
	Status = GetMotherboard().GetState<EThreadStatus>(NAME_MainBoard, NAME_None);
	if (Status == EThreadStatus::Stop)
	{
		const uint64_t ClockCounter = GetMotherboard().GetState<uint64_t>(NAME_MainBoard, NAME_None);
		if (ClockCounter != LatestClockCounter || ClockCounter == INDEX_NONE)
		{
			Load_MemorySnapshot();
			TimeElapsedCounter = ClockCounter - LatestClockCounter;
			LatestClockCounter = ClockCounter;
		}
	}

	if (TopCursorAtAddress == INDEX_NONE)
	{
		TopCursorAtAddress = GetProgramCounter();
		UserCursorAtAddress = TopCursorAtAddress;
		UserCursorAtLine = 0;
	}
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
		Input_Mouse();
		Draw_CodeDisassembler(Status);

		ImGui::End();
	}
}

FMotherboard& SDisassembler::GetMotherboard() const
{
	return *FAppFramework::Get<FAppDebugger>().Motherboard;
}

float SDisassembler::InaccessibleHeight(int32_t LineNum) const
{
	float FooterHeight = 0;
	const float HeightSeparator = ImGui::GetStyle().ItemSpacing.y;
	if (bShowStatusBar)
	{
		FooterHeight += HeightSeparator + ImGui::GetFrameHeightWithSpacing() * (float)LineNum;
	}

	return FooterHeight;
}

uint16_t SDisassembler::GetProgramCounter() const
{
	const FRegisters RegistersState = GetMotherboard().GetState<FRegisters>(NAME_MainBoard, NAME_Z80);
	return *RegistersState.PC;
}

void SDisassembler::Load_MemorySnapshot()
{
	Snapshot = GetMotherboard().GetState<FMemorySnapshot>(NAME_MainBoard, NAME_Memory);
	Memory::ToAddressSpace(Snapshot, AddressSpace);
}

void SDisassembler::Upload_MemorySnapshot()
{
	Memory::ToSnapshot(Snapshot, AddressSpace);
	GetMotherboard().SetState<FMemorySnapshot>(NAME_MainBoard, NAME_Memory, Snapshot);
}

void SDisassembler::Draw_CodeDisassembler(EThreadStatus Status)
{
	if (LatestClockCounter == INDEX_NONE)
	{
		return;
	}

	// draw memory area
	{
		bool bAccessMode;
		if (Memory::GetAccessMode(Snapshot, UserCursorAtAddress, bAccessMode))
		{
			std::string AccessModeName = bAccessMode ? "RD" : "RW";
			if (ImGui::Button(AccessModeName.c_str(), ImVec2(20.0f, 0.0f)))
			{
				Memory::SetAccessMode(Snapshot, UserCursorAtAddress, !bAccessMode);
			}
		}

		ImGui::SameLine(30);

		std::string MemoryAreaName = "Out of memory";
		Memory::GetNameByAddress(Snapshot, UserCursorAtAddress, MemoryAreaName);
		{
			ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_WEAK));
			UI::TextAligned(MemoryAreaName.c_str(), { 0.0f, 0.5f });
			ImGui::PopStyleColor();
			ImGui::SameLine(-1);
		}
	}

	// draw current clock counter
	{
		ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_WEAK));
		UI::TextAligned(std::format("CC: #{:016X}", Snapshot.SnapshotTimeCC).c_str(), { 1.0f, 0.5f });
		ImGui::PopStyleColor();
	}

	float FooterHeight = InaccessibleHeight(1);
	if (ImGui::BeginChild("##Scrolling", ImVec2(0, -FooterHeight), false,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoDecoration))
	{
		ImGui::PushFont(FFonts::Get().GetFont(FontName));
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 5.0f, 0.0f });
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

		CodeDisassemblerID = ImGui::GetCurrentWindow()->ID;

		const int32_t ColumnsNum = 3 + bMemoryArea + bShowOpcode;
		if (ImGui::BeginTable("##Disassembler", ColumnsNum,
			ImGuiTableFlags_NoPadOuterX |
			ImGuiTableFlags_NoClip |
			ImGuiTableFlags_NoBordersInBodyUntilResize |
			ImGuiTableFlags_Resizable
		))
		{
			ImGui::TableSetupColumn("Breakpoint", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_Breakpoint * CodeDisassemblerScale);
			if (bMemoryArea) ImGui::TableSetupColumn("PrefixAddress", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_PrefixAddress * CodeDisassemblerScale);
			ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_Address * CodeDisassemblerScale);
			if (bShowOpcode) ImGui::TableSetupColumn("Opcode", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, ColumnWidth_Opcode * CodeDisassemblerScale);
			ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch, ColumnWidth_Instruction * CodeDisassemblerScale);

			const int32_t Lines = UI::GetVisibleLines(FontName);
			ImGuiWindow* Window = ImGui::GetCurrentWindow();

			// handler input steps up/down
			{
				switch (InputActionEvent.Type)
				{
				case EDisassemblerInput::MouseWheelUp:
				{
					uint16_t TmpAddress = TopCursorAtAddress;
					Disassembler::StepLine(TmpAddress, -1, AddressSpace);
					TopCursorAtAddress = TmpAddress;
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::MouseWheelDown:
				{
					uint16_t TmpAddress = TopCursorAtAddress;
					Disassembler::StepLine(TmpAddress, 1, AddressSpace);
					TopCursorAtAddress = TmpAddress;
					Window->Scroll.y = 0;
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::Input_Enter:
				{
					Enter_EditColumn();
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::Input_UpArrow:
				{
					Reset_EditColumn();
					Prev_EditRow();
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::Input_DownArrow:
				{
					Reset_EditColumn();
					Next_EditRow(Lines);
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::Input_CtrlUpArrow:
				{
					Prev_EditRow(true);
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::Input_CtrlDownArrow:
				{
					Next_EditRow(Lines, true);
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::Input_CtrlLeftArrow:
				{
					Prev_EditColumn();
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::Input_CtrlRightArrow:
				{
					Next_EditColumn();
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::PageUpPressed:
				{
					uint16_t TmpAddress = TopCursorAtAddress;
					Disassembler::StepLine(TmpAddress, -Lines, AddressSpace);
					TopCursorAtAddress = TmpAddress;
					UserCursorAtAddress = TopCursorAtAddress;
					UserCursorAtLine = 0;
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::PageDownPressed:
				{
					uint16_t TmpAddress = TopCursorAtAddress;
					Disassembler::StepLine(TmpAddress, Lines, AddressSpace);
					TopCursorAtAddress = TmpAddress;
					UserCursorAtAddress = Disassembler::GetAddressToLine(TopCursorAtAddress, Lines, AddressSpace);
					UserCursorAtLine = Lines;
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				case EDisassemblerInput::GoToAddress:
				{
					int32_t GoTo_CurrentLine = 0;
					try
					{
						TopCursorAtAddress = std::any_cast<uint32_t>(InputActionEvent.Value[EDisassemblerInputValue::GoTo_Address]);
						GoTo_CurrentLine = std::any_cast<int32_t>(InputActionEvent.Value[EDisassemblerInputValue::GoTo_Line]);
					}
					catch (const std::bad_any_cast& e)
					{
						std::cout << "Error: " << e.what() << std::endl;

					}

					uint16_t TmpAddress = TopCursorAtAddress;
					Disassembler::StepLine(TmpAddress, -GoTo_CurrentLine, AddressSpace);
					TopCursorAtAddress = TmpAddress;
					UserCursorAtAddress = Disassembler::GetAddressToLine(TopCursorAtAddress, GoTo_CurrentLine, AddressSpace);
					InputActionEvent.Type = EDisassemblerInput::None;
					break;
				}
				}
			}

			// draw disassembler
			{
				const ImVec2 ContentSize = ImGui::GetWindowContentRegionMax();
				const ImVec2 ScreenPos = ImGui::GetCursorScreenPos();
				const float TextHeight = ImGui::GetTextLineHeight();
				ImDrawList* DrawList = ImGui::GetWindowDrawList();

				std::string Opcodes;
				std::string Command;
				uint16_t Address = TopCursorAtAddress;
				for (int32_t i = 0; i < Lines + 1; ++i)
				{
					const uint16_t StartAddress = Address;
					const uint32_t Length = Disassembler::Instruction(Command, Opcodes, Address, AddressSpace.data());

					ImGui::TableNextRow();

					Draw_Breakpoint(StartAddress);
					Draw_Address(StartAddress, i);
					if (bShowOpcode)
					{
						Draw_OpcodeInstruction(StartAddress, Opcodes, i);
					}
					if (Status != EThreadStatus::Run)
					{
						Draw_ProgramCounter(StartAddress);
					}
					Draw_Instruction(StartAddress, Command, i);

					// any interaction rectangle cursore
					{
						const ImVec2 Start = ImVec2(ScreenPos.x + ColumnWidth_PrefixAddress, ScreenPos.y + TextHeight * i);
						const ImVec2 End = ImVec2(Start.x + ContentSize.x - ColumnWidth_PrefixAddress, Start.y + TextHeight);
						const ImRect bb(Start, End);

						std::string UniqueID_Instructon = std::format("#{:04X}_{}", Address, Command);
						const ImGuiID GuiID = Window->GetID(UniqueID_Instructon.c_str());
						if (ImGui::ItemAdd(bb, GuiID) && (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0, true)))
						{
							UserCursorAtAddress = Disassembler::GetAddressToLine(TopCursorAtAddress, i, AddressSpace);
							UserCursorAtLine = i;
						}
					}

					// highlight the line
					if (StartAddress == UserCursorAtAddress)
					{
						const ImVec2 Start = ImVec2(ScreenPos.x + ColumnWidth_PrefixAddress, ScreenPos.y + TextHeight * i);
						const ImVec2 End = ImVec2(Start.x + ContentSize.x - ColumnWidth_PrefixAddress, Start.y + TextHeight);
						DrawList->AddRectFilled(Start, End, false ? 0x40000000 : 0x40808080);
						DrawList->AddRect(Start, End, 0x40A0A0A0, 1.0f);
					}
				}
			}
			ImGui::EndTable();
		}
		ImGui::PopStyleVar(2);
		ImGui::PopFont();
	}

	ImGui::EndChild();
	ImGui::Separator();

	// draw time passed
	{
		const double ClockFrequencyInv = GetMotherboard().GetState<double>(NAME_MainBoard, NAME_None);
		double DeltaTime = (double)TimeElapsedCounter * ClockFrequencyInv;
		std::string FormatTime;
		if (DeltaTime < 1e-7)
		{
			FormatTime = std::format("{:.2f} ns", DeltaTime * 1e9);
		}
		else if (DeltaTime < 1e-4)
		{
			FormatTime = std::format("{:.2f} us", DeltaTime * 1e6);
		}
		else if (DeltaTime < 1e-3)
		{
			FormatTime = std::format("{:.2f} ms", DeltaTime * 1e3);
		}
		else if (DeltaTime < 1e-1)
		{
			FormatTime = std::format("{:.2f} s", DeltaTime);
		}
		else
		{
			FormatTime = std::format("{:.2f} s", DeltaTime);
		}
		ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_WEAK));
		UI::TextAligned(std::format("Time elapsed: {}cc ({})", TimeElapsedCounter >> 1, FormatTime).c_str(), {0.0f, 0.5f});
		ImGui::PopStyleColor();
		ImGui::SameLine(-1);
	}

	// draw current scale
	{
		ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_WEAK));
		UI::TextAligned(std::format("{}%", 100.0f * CodeDisassemblerScale).c_str(), { 1.0f, 0.5f });
		ImGui::PopStyleColor();
	}
}

void SDisassembler::Draw_Breakpoint(uint16_t Address)
{
	ImGui::TableNextColumn();
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BREAKPOINT));
	//ImGui::TextColored(COL_CONST(UI::COLOR_REGISTERS), "");
}

void SDisassembler::Draw_Address(uint16_t Address, int32_t CurrentLine)
{
	if (bMemoryArea)
	{
		ImGui::TableNextColumn();
		ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BG_PREFIX_ADDRESS));

		ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_DISASM_ADDRESS));
		ImGui::Text("00");
		ImGui::PopStyleColor();
	}

	ImGui::TableNextColumn();
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BG_ADDRESS));

	ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_DISASM_ADDRESS));
	if (AddressEditing != Address)
	{
		ImGui::Text("#");
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::Text(FORMAT_ADDRESS(bAddressUpperCaseHex), Address);

		if ((InputActionEvent.Type == EDisassemblerInput::Input_GoToAddress && CurrentLine == 0) || 
			(ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)))
		{
			if (InputActionEvent.Type == EDisassemblerInput::Input_GoToAddress)
			{
				InputActionEvent.Type = EDisassemblerInput::None;
			}
			else
			{
				UserCursorAtAddress = Address;
			}
			bAddressEditingTakeFocus = true;
			AddressEditing = Address;
		}
	}
	else
	{
		if (bAddressEditingTakeFocus || bEditingTakeFocusReset)
		{
			ImGui::SetKeyboardFocusHere(0);
			std::sprintf(AddressInputBuffer, FORMAT_ADDRESS(bAddressUpperCaseHex), Address);

			if (bEditingTakeFocusReset)
			{
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				if (!window->SkipItems)
				{
					const ImGuiID id = window->GetID("##address");
					ImGuiInputTextState* State = ImGui::GetInputTextState(id);
					State->ReloadUserBufAndSelectAll();
				}
				bEditingTakeFocusReset = false;
			}
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::Text("#");
		ImGui::SameLine(0.0f, 0.0f);

		struct FUserData
		{
			static int Callback(ImGuiInputTextCallbackData* Data)
			{
				FUserData* UserData = (FUserData*)Data->UserData;
				if (!Data->HasSelection())
				{
					UserData->CursorPos = Data->CursorPos;
				}
				return 0;
			}
			char   CurrentBufOverwrite[5];  // Input
			int    CursorPos;               // Output
		} UserData;

		UserData.CursorPos = -1;
		memcpy(&UserData.CurrentBufOverwrite, AddressInputBuffer, 5);

		bool AddressWrite = false;
		static const ImGuiInputTextFlags Flags = ImGuiInputTextFlags_CharsHexadecimal |
												 ImGuiInputTextFlags_CharsUppercase |
												 ImGuiInputTextFlags_EnterReturnsTrue |
												 ImGuiInputTextFlags_NoHorizontalScroll |
												 ImGuiInputTextFlags_CallbackAlways |
												 ImGuiInputTextFlags_AlwaysOverwrite;

		const ImVec2 StringSize = ImGui::CalcTextSize(AddressInputBuffer, nullptr, true);
		if (ImGui::InputTextEx("##address", NULL, AddressInputBuffer, 5, StringSize, Flags, FUserData::Callback, &UserData))
		{
			AddressWrite = true;
		}
		else if (!bAddressEditingTakeFocus && !ImGui::IsItemActive())
		{
			AddressEditing = INDEX_NONE;
		}

		bAddressEditingTakeFocus = false;

		uint32_t AddressInputValue = 0;
		if (AddressWrite && std::sscanf(AddressInputBuffer, "%X", &AddressInputValue) == 1)
		{
			AddressWrite = false;
			AddressEditing = INDEX_NONE;

			InputActionEvent.Type = EDisassemblerInput::GoToAddress;
			InputActionEvent.Value[EDisassemblerInputValue::GoTo_Address] = AddressInputValue;
			InputActionEvent.Value[EDisassemblerInputValue::GoTo_Line] = CurrentLine;
		}

		ImGui::PopStyleVar(2);
	}

	ImGui::PopStyleColor();
}

void SDisassembler::Draw_OpcodeInstruction(uint16_t Address, const std::string& Opcodes, int32_t CurrentLine)
{

	ImGui::TableNextColumn();
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BG_OPCODE_ADDRESS));

	ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_DISASM_ADDRESS));
	ON_SCOPE_EXIT
	{
		ImGui::PopStyleColor();;
	};

	ImGuiWindow* Window = ImGui::GetCurrentWindow();
	if (OpcodeInstructionEditing != Address)
	{
		const ImVec2 Padding = { 0.0f, 0.0f };
		const ImVec2 Aligment = { 0.0f, 0.0f };
		const ImVec2 Position = Window->DC.CursorPos;

		const ImVec2 StringSize = ImGui::CalcTextSize(Opcodes.c_str(), nullptr, true);
		const ImVec2 Size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), StringSize.x + Padding.x * 2.0f, StringSize.y + Padding.y * 2.0f);
		const ImRect bb(Position, Position + Size);
		// interaction rectangle
		{
			std::string UniqueID_Instructon = std::format("#{:04X}_{}", Address, Opcodes);
			const ImGuiID GuiID = Window->GetID(UniqueID_Instructon.c_str());
			if (!ImGui::ItemAdd(bb, GuiID))
			{
				return;
			}
		}

		ImGui::RenderTextClipped(bb.Min + Padding, bb.Max - Padding, Opcodes.c_str(), nullptr, &StringSize, Aligment, &bb);

		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
		{
			bOpcodeInstructionEditingTakeFocus = true;
			OpcodeInstructionEditing = Address;
			UserCursorAtAddress = Address;
			UserCursorAtColumn = DisassemblerColumn::Opcode;
		}
	}
	else
	{
		if (bOpcodeInstructionEditingTakeFocus || bEditingTakeFocusReset)
		{
			ImGui::SetKeyboardFocusHere(0);
			assert(Opcodes.size() < 256);
			std::memcpy(OpcodeInstructioBuffer, Opcodes.data(), FMath::Min<size_t>(Opcodes.size() + 1, 256));

			if (bEditingTakeFocusReset)
			{
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				if (!window->SkipItems)
				{
					const ImGuiID id = window->GetID("##opcode");
					ImGuiInputTextState* State = ImGui::GetInputTextState(id);
					State->ReloadUserBufAndSelectAll();
				}
				bEditingTakeFocusReset = false;
			}
		}

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

		struct FUserData
		{
			static int Callback(ImGuiInputTextCallbackData* Data)
			{
				FUserData* UserData = (FUserData*)Data->UserData;
				if (!Data->HasSelection())
				{
					UserData->CursorPos = Data->CursorPos;
				}
				return 0;
			}
			char   CurrentBufOverwrite[256];// Input
			int    CursorPos;               // Output
		} UserData;

		UserData.CursorPos = -1;
		memcpy(&UserData.CurrentBufOverwrite, OpcodeInstructioBuffer, 256);

		static const ImGuiInputTextFlags Flags = 
			ImGuiInputTextFlags_CharsHexadecimal |
			ImGuiInputTextFlags_CharsUppercase |
			ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_CallbackAlways |
			ImGuiInputTextFlags_AlwaysOverwrite;

		const float TextHeight = ImGui::GetTextLineHeight();
		const ImVec2 Size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), ColumnWidth_Opcode, TextHeight);
		if (ImGui::InputTextEx("##opcode", NULL, OpcodeInstructioBuffer, 256, Size, Flags, FUserData::Callback, &UserData))
		{
			uint8_t TmpNumber;
			std::vector<uint8_t> Opcodes;
			for (size_t i = 0; i < std::strlen(OpcodeInstructioBuffer); i += 2)
			{
				if (i + 2 <= std::strlen(OpcodeInstructioBuffer) && std::sscanf(OpcodeInstructioBuffer + i, "%2hhX", &TmpNumber) == 1)
				{
					Opcodes.push_back(TmpNumber);
				}
			}
			if (Address + Opcodes.size() <= AddressSpace.size())
			{
				std::ranges::copy(Opcodes.begin(), Opcodes.end(), AddressSpace.begin() + Address);
			}
		}
		else if (!bOpcodeInstructionEditingTakeFocus && !ImGui::IsItemActive())
		{
			OpcodeInstructionEditing = INDEX_NONE;
		}
		bOpcodeInstructionEditingTakeFocus = false;

		ImGui::PopStyleVar(2);
	}
}

void SDisassembler::Draw_Instruction(uint16_t Address, const std::string& _Command, int32_t CurrentLine)
{
	std::string Command = _Command;
	std::vector<std::string> Instruction;

	ImGui::TableNextColumn();
	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, COL_CONST32(UI::COLOR_DISASM_BG_MNEMONIC));

	if (/*InstructionEditing != Address*/true)
	{
		ImGuiWindow* Window = ImGui::GetCurrentWindow();

		const float FirstPadding = 10.0f;
		const float SecondPadding = FirstPadding + 80.0f;
		const ImVec2 Padding = { 0.0f, 0.0f };
		const ImVec2 Aligment = { 0.0f, 0.0f };
		const ImVec2 Position = Window->DC.CursorPos;

		// interaction rectangle
		{
			const ImVec2 StringSize = ImGui::CalcTextSize(Command.c_str(), nullptr, true);
			const ImVec2 Size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), StringSize.x + Padding.x * 2.0f, StringSize.y + Padding.y * 2.0f);
			const ImRect bb(Position, Position + Size);

			std::string UniqueID_Instructon = std::format("#{:04X}_{}", Address, Command);
			const ImGuiID GuiID = Window->GetID(UniqueID_Instructon.c_str());
			if (!ImGui::ItemAdd(bb, GuiID))
			{
				return;
			}
		}

		// draw mnemonic
		{
			ImGui::PushStyleColor(ImGuiCol_Text, COL_CONST(UI::COLOR_DISASM_INSTRUCTION));

			std::string Operation = Disassembler::Operation(Command);

			const ImVec2 StringSize = ImGui::CalcTextSize(Operation.c_str(), nullptr, true);
			const ImVec2 Size = ImGui::CalcItemSize(ImVec2(-FLT_MIN, 0.0f), StringSize.x + Padding.x * 2.0f, StringSize.y + Padding.y * 2.0f);
			const ImRect bb(Position, Position + Size);
			ImGui::RenderTextClipped(bb.Min + Padding + ImVec2(FirstPadding, 0.0f) * CodeDisassemblerScale, bb.Max - Padding, Operation.c_str(), nullptr, &StringSize, Aligment, &bb);

			ImGui::PopStyleColor();
		}

		// draw remaining part of command
		{
			float TextOffset = 0;
			ImGuiStyle& Style = GImGui->Style;
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

		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
		{
			bInstructionEditingTakeFocus = true;
			InstructionEditing = Address;
			UserCursorAtAddress = Address;
			//UserCursorAtColumn = DisassemblerColumn::Instruction;
		}
	}
	else
	{

	}
}

void SDisassembler::Draw_ProgramCounter(uint16_t Address)
{
	if (GetProgramCounter() != Address)
	{
		return;
	}

	ImGuiWindow* Window = ImGui::GetCurrentWindow();
	if (Window->SkipItems)
	{
		return;
	}
	FImage Image = FImageBase::Get().GetImage(ImageProgramCounter);
	const ImVec2 Size = ImVec2(Image.Width, Image.Height) * 0.3f * CodeDisassemblerScale;

	const float OffsetX = ColumnWidth_Opcode;
	const float OffsetY = (ImGui::GetTextLineHeight() - Size.y) * 0.5f;

	const ImVec2 Position = ImVec2(Window->DC.CursorPos.x, Window->DC.CursorPosPrevLine.y) + ImVec2(OffsetX, OffsetY) * CodeDisassemblerScale;
	const ImRect bb(Position, Position + Size);
	Window->DrawList->AddImage(Image.ShaderResourceView, bb.Min, bb.Max, ImVec2(0.0, 0.0f), ImVec2(1.0, 1.0f), COL_CONST32(UI::COLOR_WHITE));
}

void SDisassembler::Enter_EditColumn()
{
	Reset_EditColumn();

	if (UserCursorAtColumn == DisassemblerColumn::None)
	{
		UserCursorAtColumn = DisassemblerColumn::Address;
	}

	switch (UserCursorAtColumn)
	{
		case DisassemblerColumn::Address:
		{
			bAddressEditingTakeFocus = true;
			AddressEditing = UserCursorAtAddress;
			break;
		}
		case DisassemblerColumn::Opcode:
		{
			
			bOpcodeInstructionEditingTakeFocus = true;
			OpcodeInstructionEditing = UserCursorAtAddress;
			break;
		}
		case DisassemblerColumn::Instruction:
		{
			bInstructionEditingTakeFocus = true;
			InstructionEditing = UserCursorAtAddress;
			break;
		}
	}
}

void SDisassembler::Reset_EditColumn()
{
	bAddressEditingTakeFocus = 
	bOpcodeInstructionEditingTakeFocus =
	bInstructionEditingTakeFocus = false;

	AddressEditing =
	OpcodeInstructionEditing =
	InstructionEditing = INDEX_NONE;
}

void SDisassembler::Prev_EditRow(bool bCtrl /*= false*/)
{
	uint16_t TmpAddress;
	if (UserCursorAtLine <= 0)
	{
		TmpAddress = TopCursorAtAddress;
		Disassembler::StepLine(TmpAddress, -1, AddressSpace);
		UserCursorAtAddress = TopCursorAtAddress = TmpAddress;
	}
	else
	{
		TmpAddress = UserCursorAtAddress;
		Disassembler::StepLine(TmpAddress, -1, AddressSpace);
		UserCursorAtAddress = TmpAddress;
		UserCursorAtLine--;
	}

	if (!bCtrl)
	{
		return;
	}

	switch (UserCursorAtColumn)
	{
		case DisassemblerColumn::Address:
		{
			AddressEditing = UserCursorAtAddress;
			break;
		}
		case DisassemblerColumn::Opcode:
		{
			OpcodeInstructionEditing = UserCursorAtAddress;
			break;
		}
		case DisassemblerColumn::Instruction:
		{
			InstructionEditing = UserCursorAtAddress;
			break;
		}
	}

	bEditingTakeFocusReset = true;
}

void SDisassembler::Next_EditRow(int32_t MaxLines, bool bCtrl /*= false*/)
{
	uint16_t TmpAddress;
	if (UserCursorAtLine >= MaxLines)
	{
		TmpAddress = TopCursorAtAddress;
		Disassembler::StepLine(TmpAddress, 1, AddressSpace);
		TopCursorAtAddress = TmpAddress;
		UserCursorAtAddress = Disassembler::GetAddressToLine(TopCursorAtAddress, MaxLines, AddressSpace);
	}
	else
	{
		TmpAddress = UserCursorAtAddress;
		Disassembler::StepLine(TmpAddress, 1, AddressSpace);
		UserCursorAtAddress = TmpAddress;
		UserCursorAtLine++;
	}

	if (!bCtrl)
	{
		return;
	}

	switch (UserCursorAtColumn)
	{
	case DisassemblerColumn::Address:
	{
		AddressEditing = UserCursorAtAddress;
		break;
	}
	case DisassemblerColumn::Opcode:
	{
		OpcodeInstructionEditing = UserCursorAtAddress;
		break;
	}
	case DisassemblerColumn::Instruction:
	{
		InstructionEditing = UserCursorAtAddress;
		break;
	}
	}

	bEditingTakeFocusReset = true;
}

void SDisassembler::Prev_EditColumn()
{
	if (UserCursorAtColumn == DisassemblerColumn::None)
	{
		//UserCursorAtColumn = DisassemblerColumn::Instruction;
		UserCursorAtColumn = DisassemblerColumn::Opcode;
		return;
	}

	switch (UserCursorAtColumn)
	{
		//case DisassemblerColumn::Address:
		//{
		//	bOpcodeInstructionEditingTakeFocus = true;
		//	OpcodeInstructionEditing = AddressEditing;
		//	AddressEditing = INDEX_NONE;
		//	break;
		//}
		case DisassemblerColumn::Opcode:
		{
			bAddressEditingTakeFocus = true;
			AddressEditing = OpcodeInstructionEditing;
			OpcodeInstructionEditing = INDEX_NONE;
			break;
		}
		case DisassemblerColumn::Instruction:
		{
			break;
		}
	}

	if (--UserCursorAtColumn <= DisassemblerColumn::Address)
	{
		UserCursorAtColumn = DisassemblerColumn::Address;
	}
}

void SDisassembler::Next_EditColumn()
{
	if (UserCursorAtColumn == DisassemblerColumn::None)
	{
		UserCursorAtColumn = DisassemblerColumn::Address;
		return;
	}

	switch (UserCursorAtColumn)
	{
		case DisassemblerColumn::Address:
		{
			bOpcodeInstructionEditingTakeFocus = true;
			OpcodeInstructionEditing = AddressEditing;
			AddressEditing = INDEX_NONE;
			break;
		}
		case DisassemblerColumn::Opcode:
		{
			break;
		}
		case DisassemblerColumn::Instruction:
		{
			break;
		}
	}
	
	if (++UserCursorAtColumn >= DisassemblerColumn::Opcode)
	{
		UserCursorAtColumn = DisassemblerColumn::Opcode;
	}
}

void SDisassembler::Input_HotKeys()
{
	static std::vector<FHotKey> Hotkeys =
	{
		{ ImGuiKey_Enter,							ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_Enter, this)						},	// debugger: enter
		{ ImGuiMod_Alt | ImGuiKey_KeypadMultiply,	ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_ShowNextStatement, this)			},	// debugger: show next statement	(alt + Num*)
		{ ImGuiKey_UpArrow,							ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_UpArrow, this)						},	// debugger: up arrow
		{ ImGuiKey_DownArrow,						ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_DownArrow, this)					},	// debugger: down arrow
		{ ImGuiMod_Ctrl | ImGuiKey_UpArrow,			ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_CtrlUpArrow, this)					},	// debugger: edit up				(ctrl + up arrow)
		{ ImGuiMod_Ctrl | ImGuiKey_DownArrow,		ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_CtrlDownArrow, this)				},	// debugger: edit down				(ctrl + down arrow)
		{ ImGuiMod_Ctrl | ImGuiKey_LeftArrow,		ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_CtrlLeftArrow, this)				},	// debugger: edit left				(ctrl + left arrow)
		{ ImGuiMod_Ctrl | ImGuiKey_RightArrow,		ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_CtrlRightArrow, this)				},	// debugger: edit right				(ctrl + right arrow)
		{ ImGuiKey_PageUp,							ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_PageUp, this)						},	// debugger: page up
		{ ImGuiKey_PageDown,						ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_PageDown, this)						},	// debugger: page down

		{ ImGuiMod_Ctrl | ImGuiKey_G,				ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_GoToAddress, this)					},	// debugger: go to address

		{ ImGuiKey_F5,								ImGuiInputFlags_Repeat, std::bind(&ThisClass::Input_Step, this, FCPU_StepType::StepTo)	},	// debugger: step into				(f4)
		{ ImGuiKey_F7,								ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_Step, this, FCPU_StepType::StepInto)},	// debugger: step into				(f7)
		{ ImGuiKey_F8,								ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_Step, this, FCPU_StepType::StepOver)},	// debugger: step over				(f8)
		{ ImGuiKey_F11,								ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Input_Step, this, FCPU_StepType::StepOut)	},	// debugger: step out				(f11)
	};

	Shortcut::Handler(Hotkeys);
}

void SDisassembler::Input_Step(FCPU_StepType Type)
{
	Upload_MemorySnapshot();
	GetMotherboard().Input_Step(Type);

	const uint16_t PC = GetProgramCounter();
	const int32_t Lines = UI::GetVisibleLines(FontName, InaccessibleHeight(2));
	if (!Disassembler::IsAddressInArea(PC, TopCursorAtAddress, AddressSpace, Lines))
	{
		TopCursorAtAddress = INDEX_NONE;
	}
}

void SDisassembler::Input_Mouse()
{
	ImGuiContext& Context = *GImGui;
	const float MouseWheel = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner) ? Context.IO.MouseWheel : 0.0f;

	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != CodeDisassemblerID)
	{
		return;
	}

	if (MouseWheel != 0.0f && Context.IO.KeyCtrl && !Context.IO.FontAllowUserScaling)
	{
		CodeDisassemblerScale += MouseWheel * 0.0625f;
		CodeDisassemblerScale = FFonts::Get().SetSize(FontName, CodeDisassemblerScale, 0.5f, 1.5f);
		InputActionEvent.Type = EDisassemblerInput::None;
	}
	else if (MouseWheel != 0)
	{
		InputActionEvent.Type = MouseWheel > 0 ? EDisassemblerInput::MouseWheelUp : EDisassemblerInput::MouseWheelDown;
		InputActionEvent.Value[EDisassemblerInputValue::ScrollingLines] = 1;
	}
}

void SDisassembler::Input_Enter()
{
	InputActionEvent.Type = EDisassemblerInput::Input_Enter;
}

void SDisassembler::Input_ShowNextStatement()
{
	TopCursorAtAddress = INDEX_NONE;
}

void SDisassembler::Input_UpArrow()
{
	InputActionEvent.Type = EDisassemblerInput::Input_UpArrow;
}

void SDisassembler::Input_DownArrow()
{
	InputActionEvent.Type = EDisassemblerInput::Input_DownArrow;
}

void SDisassembler::Input_CtrlUpArrow()
{
	InputActionEvent.Type = EDisassemblerInput::Input_CtrlUpArrow;
}

void SDisassembler::Input_CtrlDownArrow()
{
	InputActionEvent.Type = EDisassemblerInput::Input_CtrlDownArrow;
}

void SDisassembler::Input_CtrlLeftArrow()
{
	InputActionEvent.Type = EDisassemblerInput::Input_CtrlLeftArrow;
}

void SDisassembler::Input_CtrlRightArrow()
{
	InputActionEvent.Type = EDisassemblerInput::Input_CtrlRightArrow;
}

void SDisassembler::Input_PageUp()
{
	InputActionEvent.Type = EDisassemblerInput::PageUpPressed;
}

void SDisassembler::Input_PageDown()
{
	InputActionEvent.Type = EDisassemblerInput::PageDownPressed;
}

void SDisassembler::Input_GoToAddress()
{
	InputActionEvent.Type = EDisassemblerInput::Input_GoToAddress;
}

void SDisassembler::OnInputDebugger(bool bDebuggerState)
{
	if (bDebuggerState /*true = enter debugger*/)
	{
		Upload_MemorySnapshot();
	}
	else
	{
		TopCursorAtAddress = INDEX_NONE;
	}
}
