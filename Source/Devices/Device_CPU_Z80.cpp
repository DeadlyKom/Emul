#include "Device_CPU_Z80.h"

namespace
{
	static const FName CPU_Z80_DeviceName = "CPU Z80";
}

FCPU_Z80::FCPU_Z80()
	: FDevice(CPU_Z80_DeviceName, EDeviceType::CPU)
{}

void FCPU_Z80::Reset()
{
	auto a = sizeof(Registers);
	memset(&Registers, 0, sizeof(Registers));
}

void FCPU_Z80::ApplySignals(uint64_t SignalsBus)
{
	//// set all control signals to HIGH level except the Z80_RESET signal
	//Z80_MAKE_CTRL(InternalSignalBus, Z80_SYS_CTRL_PIN_MASK, (Z80_HALT | Z80_WAIT | Z80_INT | Z80_NMI), Z80_BUS_CTRL_PIN_MASK);
	//
	//// set address signal
	//Z80_SET_ADDR(InternalSignalBus, 0x0000);
	//
	//// set data signal
	//Z80_SET_DATA(InternalSignalBus, 0x00);
}
