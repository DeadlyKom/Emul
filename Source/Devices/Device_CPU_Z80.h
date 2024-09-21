#pragma once

#include <CoreMinimal.h>
#include "Devices/Device.h"
#include "Utils/Register.h"
#include "Device_CPU_Z80_define.h"

class FCPU_Z80 : public FDevice
{
public:
	FCPU_Z80();
	virtual FName GetName() override;

private:
	struct
	{
		Register16 PC;		// program counter
		Register16 IR;		// internal register to fetch instructions

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
		Register16 WZ;		// temporary address reg
		bool IFF1;
		bool IFF2;
	} Registers;
};
