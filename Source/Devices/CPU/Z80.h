#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"
#include "Utils/SignalsBus.h"
#include "Utils/Register.h"

class FSignalsBus;

class FCPU_Z80 : public FDevice
{
	using ThisClass = FCPU_Z80;
public:
	FCPU_Z80();

	virtual void Tick(FClockGenerator& CG, FSignalsBus& SB) override;
	virtual void Reset() override;

private:
	void InstructionOpcodeFetch(FClockGenerator& CG, FSignalsBus& SB);

	struct
	{
		Register16 PC;		// program counter
		RegisterIR IR;		// internal register to fetch instructions

		Register16 IX;		// index register
		Register16 IY;		// index register
		Register16 SP;		// stack pointer

		// main register set
		RegisterAF AF;		// accumulator & flags
		Register16 HL;		// register pair HL
		Register16 DE;		// register pair DE
		Register16 BC;		// register pair BC

		// alternate register set
		RegisterAF AF_;		// accumulator & flags
		Register16 HL_;		// register pair HL'
		Register16 DE_;		// register pair DE'
		Register16 BC_;		// register pair BC'

		// internal
		uint8_t IM;			// maskable interrupt mode
		uint8_t CC;			// clock cycles in one machine cycle
		uint8_t MC;			// machine cycle counter
		uint8_t Step;       // the currently active decoder step
		uint8_t Opcode;     // current opcode
		Register16 WZ;		// temporary address reg
		bool IFF1;
		bool IFF2;
	} Registers;
};
