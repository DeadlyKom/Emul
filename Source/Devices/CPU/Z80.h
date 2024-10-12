#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"
#include "Utils/Register.h"
#include "Utils/Pipeline.h"
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

		T_WAIT,

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
	bool bIFF1;
	bool bIFF2;
	bool bInitiCPU;				// flag indicating to initialize CPU
	bool bInstrCycleDone;		// flag indicating the end instruction cycle ended
	bool bInstrExeDone;			// flag indicating the instruction execution completed
	bool bOpcodeDecoded;		// flag indicating that the opcode has been decoded
	uint8_t IM;					// maskable interrupt mode
	Register16 WZ;				// temporary register

	uint8_t Opcode;				// current opcode
	uint8_t DataLatch;			// temporary store for data bus value
	uint32_t CC;				// counter of clock cycles in one machine cycle
	MachineCycle::Type MC;		// machine cycle counter
	MachineCycle::Type NMC;		// next machine cycle
	Prefix::Type Prefix;

	DecoderStep::Type DSCP;		// the currently active decoder step for command pipeline
	DecoderStep::Type DSTP;		// the currently active decoder step for tick pipeline

	FPipeline CP;				// command pipeline
	FPipeline TP;				// tick pipeline
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
	virtual bool IsInstrCycleDone() const override { return Registers.bInstrCycleDone; }
	virtual bool IsInstrExeDone() const override { return Registers.bInstrExeDone; }

	void Cycle_Reset();
	void Cycle_InitCPU();
	void Cycle_OpcodeFetch(FCPU_Z80& CPU);
	void Cycle_MemoryRead(uint16_t Address, Register8& Register);
	void Cycle_MemoryWrite(uint16_t Address, Register8& Register, std::function<void(FCPU_Z80& CPU)>&& CompletedCallback = nullptr);

	FInternalRegisters Registers;

private:
	void OpcodeDecode();

	static const CMD_FUNC Unprefixed[256];
	std::function<void(FCPU_Z80& CPU)> Execute_Cycle;
	std::function<void(FCPU_Z80& CPU)> Execute_Tick;
};
