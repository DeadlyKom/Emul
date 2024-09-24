#include "Device_Memory.h"

namespace
{
	static const FName Memory_DeviceName = "Memory";
}

FMemory::FMemory()
	: FDevice(Memory_DeviceName, EDeviceType::Memory)
{}

void FMemory::Tick(FClockGenerator& CG, uint64_t& InOutISB)
{
}
