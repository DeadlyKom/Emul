#include "Device_CPU_Z80.h"

namespace
{
	static const FName CPU_Z80_DeviceName = "CPU Z80";
}

FCPU_Z80::FCPU_Z80()
	: FDevice(CPU_Z80_DeviceName)
{}

FName FCPU_Z80::GetName()
{
	return DeviceName;
}
