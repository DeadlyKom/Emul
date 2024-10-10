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
};

class ICPU_Z80
{
public:
	ICPU_Z80() {};
	virtual ~ICPU_Z80() = default;
	virtual FRegisters GetRegisters() const = 0;
};
