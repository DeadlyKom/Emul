#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"
#include "Utils/Register.h"
#include "Utils/CommandPipeline.h"

class FSignalsBus;
typedef void (*CMD_FUNC)(FCPU_Z80&);

namespace DecoderStep
{
	enum Type : int32_t
	{
		// half-clock tick
		T1_H1 = 0,
		T1_H2,
		T2_H1,
		T2_H2,
		T3_H1,
		T3_H2,
		T4_H1,
		T4_H2,
		T5_H1,
		T5_H2,
		T6_H1,
		T6_H2,

		Completed = -1,
	};
}

namespace MachineCycle
{
	enum Type : int32_t
	{
		None = 0,
		M1,
		M2,
		M3,
		M4,
		M5,
		M6,
	};
}

inline MachineCycle::Type& operator++(MachineCycle::Type& Value)
{
	return reinterpret_cast<MachineCycle::Type&>(++reinterpret_cast<int32_t&>(Value));
}

inline MachineCycle::Type operator++(MachineCycle::Type& Value, int32_t)
{
	return ++Value;
}

namespace Prefix
{
	enum Type : int32_t
	{
		None = 0,
		CB,
		ED,
		DD,
		FD,
		DDCB,
		FDCB,
	};
}

struct FRegisters
{
	Register16 PC;		// program counter
	RegisterIR IR;		// internal register to fetch instructions

	Register16 IX;		// index register
	Register16 IY;		// index register
	Register16 SP;		// stack pointer

	// main register set
	RegisterAF AF;		// register pair AF
	Register16 HL;		// register pair HL
	Register16 DE;		// register pair DE
	Register16 BC;		// register pair BC

	// alternate register set
	RegisterAF AF_;		// register pair AF'
	Register16 HL_;		// register pair HL'
	Register16 DE_;		// register pair DE'
	Register16 BC_;		// register pair BC'

	// internal
	uint8_t IM;			// maskable interrupt mode
	Register16 WZ;		// temporary register
	bool IFF1;
	bool IFF2;

	uint32_t CC;			// counter of clock cycles in one machine cycle
#ifndef NDEBUG
	MachineCycle::Type MC;	// machine cycle counter
	DecoderStep::Type Step;	// the currently active decoder step
	Prefix::Type Prefix;
#else
	int32_t MC;
	int32_t Step;
	int32_t Prefix;
#endif
	FCommandPipeline CP;
};

class FCPU_Z80 : public FDevice
{
	using ThisClass = FCPU_Z80;
public:
	FCPU_Z80();

	virtual void Tick() override;
	virtual void Reset() override;

	void InstructionFetch();
	void MemoryReadCycle(uint16_t Address, Register8& Register, std::function<void(FCPU_Z80& CPU)>&& CompletedCallback = nullptr);

	FRegisters Registers;

private:
	void DecodeAndExecuteFetchedInstruction(uint8_t Opcode);

	static const CMD_FUNC Unprefixed[256];
	std::function<void(FCPU_Z80& CPU)> Command;
};
