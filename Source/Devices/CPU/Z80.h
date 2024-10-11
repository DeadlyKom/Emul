#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"
#include "Utils/Register.h"
#include "Utils/CommandPipeline.h"
#include "Interface_CPU_Z80.h"

class FSignalsBus;

namespace DecoderStep
{
	enum Type : int32_t
	{
		// half-clock tick
		T1_H1	= 0,
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

		TW_H1,
		TW_H2,

		T1		= 0,
		T2,
		T3,
		T4,
		T5,
		T6,

		Completed = -1,
	};
}

typedef void (*CMD_FUNC)(FCPU_Z80&);
typedef void (*CMD_FUNC_CYCLE)(FCPU_Z80&, DecoderStep::Type Step);

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

struct FInternalRegisters : public FRegisters
{
	// internal
	uint8_t IM;			// maskable interrupt mode
	Register16 WZ;		// temporary register
	bool IFF1;
	bool IFF2;

	uint8_t Opcode;			// current opcode
	uint8_t DataLatch;		// temporary store for data bus value
	uint32_t CC;			// counter of clock cycles in one machine cycle
	MachineCycle::Type MC;	// machine cycle counter
	MachineCycle::Type NMC;	// next machine cycle
	DecoderStep::Type Step;	// the currently active decoder step
	Prefix::Type Prefix;

	FCommandPipeline CP;
};

class FCPU_Z80 : public FDevice, public ICPU_Z80
{
	using ThisClass = FCPU_Z80;
public:
	FCPU_Z80();
	virtual ~FCPU_Z80() = default;

	virtual void Tick() override;
	virtual void Reset() override;
	virtual FRegisters GetRegisters() const override;

	void Cycle_InstructionFetch();
	void Cycle_MemoryRead(uint16_t Address, Register8& Register, std::function<void(FCPU_Z80& CPU)>&& CompletedCallback = nullptr);
	void Cycle_MemoryWrite(uint16_t Address, Register8& Register, std::function<void(FCPU_Z80& CPU)>&& CompletedCallback = nullptr);

	// ALU operation of adding the Program Counter and relative offset using the intermediate register WZ
	void Cycle_ALU_LoadWZ_AddWZ_UnloadWZ();

	FInternalRegisters Registers;

private:
	void Cycle_Reset();
	void DecodeAndAddInstruction(uint8_t Opcode);
	void CycleExecuteFetchedInstruction(uint8_t Opcode, DecoderStep::Type Step);

	static const CMD_FUNC Unprefixed[256];
	static const CMD_FUNC_CYCLE Unprefixed_Cycle[256];
	std::function<void(FCPU_Z80& CPU)> Command;
};
