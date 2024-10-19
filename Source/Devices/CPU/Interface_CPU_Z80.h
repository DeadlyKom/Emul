#pragma once

#include "Utils/Register.h"

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

	bool bIFF1;
	bool bIFF2;
	uint8_t IM;			// maskable interrupt mode
};

class ICPU_Z80
{
public:
	ICPU_Z80() = default;
	virtual ~ICPU_Z80() = default;

	virtual bool Flush() = 0;
	virtual double GetFrequency() const = 0;
	virtual FRegisters GetRegisters() const = 0;
	virtual bool IsInstrCycleDone() const = 0;
	virtual bool IsInstrExecuteDone() const = 0;
	virtual std::ostream& Serialize(std::ostream& os) const = 0;
	virtual std::istream& Deserialize(std::istream& is) = 0;
};
