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

		// clock tick overlap stage
		OLP1_H1,
		OLP1_H2,
		OLP2_H1,
		OLP2_H2,
		OLP3_H1,
		OLP3_H2,
		OLP4_H1,
		OLP4_H2,	// none

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

enum class EPrefix
{
	None = 0,
	CB,
	ED,
	DD,
	FD,
	DDCB,
	FDCB,
};

namespace ERegMap
{
	enum Type : uint8_t
	{
		HL = 0,
		IX,
		IY,
		MAX,
	};
}

struct FInternalRegisters : public FRegisters
{
	// internal
	bool bInitiCPU;					// flag indicating to initialize CPU
	bool bInstrCycleDone;			// flag indicating the end instruction cycle ended
	bool bInstrCompleted;			// flag indicating that tick pipeline instruction is completed
	bool bNextTickPipeline;			// flag indicating the fetch of next tick pipeline
	bool bOpcodeDecoded;			// flag indicating that the opcode has been decoded

	RegisterF Flags;				// previous operation status flags
	Register8 LBUS;					// temporary store for low bus value
	Register8 HBUS;					// temporary store for hight bus value
	Register8 ALU_BUS;				// temporary store for alu bus value
	Register16 WZ;					// temporary register
	Register16 RegisterLatch;

	uint8_t Opcode;					// current opcode
	uint32_t CC;					// counter of clock cycles in one machine cycle
	MachineCycle::Type MC;			// machine cycle counter
	MachineCycle::Type NMC;			// next machine cycle
	EPrefix Prefix;
	ERegMap::Type Map;

	DecoderStep::Type DSCP;		// the currently active decoder step for command pipeline
	DecoderStep::Type DSTP;		// the currently active decoder step for tick pipeline

	FPipeline CP;				// command pipeline
	FPipeline TP;				// tick pipeline

	friend std::ostream& operator<<(std::ostream& os, const FInternalRegisters& Registers)
	{
		os.write(reinterpret_cast<const char*>(&Registers), sizeof(FInternalRegisters));
		return os;
	}
	friend std::istream& operator>>(std::istream& is, FInternalRegisters& Registers)
	{
		is.read(reinterpret_cast<char*>(&Registers), sizeof(FInternalRegisters));
		return is;
	}
};

class FCPU_Z80 : public FDevice, public ICPU_Z80
{
	using ThisClass = FCPU_Z80;
public:
	FCPU_Z80(double _Frequency);
	virtual ~FCPU_Z80() = default;

	virtual void Tick() override;
	virtual void Reset() override;
	virtual void CalculateFrequency(double MainFrequency) override;

	virtual bool Flush() override;
	virtual double GetFrequency() const override;
	virtual FRegisters GetRegisters() const override;
	virtual bool IsInstrCycleDone() const override { return Registers.bInstrCycleDone; }
	virtual bool IsInstrExecuteDone() const override { return Registers.bInstrCompleted; }
	virtual std::ostream& Serialize(std::ostream& os) const override { os << Registers; return os; }
	virtual std::istream& Deserialize(std::istream& is) override { is >> Registers; return is; }

	void Cycle_Reset();
	void Cycle_InitCPU();
	void Cycle_OpcodeFetch(FCPU_Z80& CPU);
	void Cycle_MemoryRead(uint16_t Address, Register8& Register, int32_t Delay = 0);
	void Cycle_MemoryWrite(uint16_t Address, Register8& Register);

	FInternalRegisters Registers;

private:
	void OpcodeDecode();

	static const CMD_FUNC Unprefixed[256];
	std::function<void(FCPU_Z80& CPU)> Execute_Cycle;
	std::function<void(FCPU_Z80& CPU)> Execute_Tick;
};
