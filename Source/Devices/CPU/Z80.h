#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"
#include "Utils/SignalsBus.h"
#include "Utils/Register.h"

class FSignalsBus;
namespace DecoderStep { enum Type : uint32_t; }
namespace MachineCycle { enum Type : int32_t; }

class FCPU_Z80 : public FDevice
{
	using ThisClass = FCPU_Z80;
public:
	FCPU_Z80();

	virtual void Tick() override;
	virtual void Reset() override;

private:
	void InstructionFetch();
	void MemoryReadCycle();

	struct
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
		Register16 WZ;		// temporary address register
		bool IFF1;
		bool IFF2;

		uint8_t Opcode;			// fetched opcode
		uint32_t CC;			// counter of clock cycles in one machine cycle
#ifndef NDEBUG
		MachineCycle::Type MC;	// machine cycle counter
		DecoderStep::Type Step;	// the currently active decoder step
#else
		uint32_t MC;
		uint32_t Step;
#endif 

	} Registers;
};
