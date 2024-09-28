#include "Z80.h"
#include "Utils/SignalsBus.h"
#include "Motherboard/Motherboard_ClockGenerator.h"

static void CMD_PC_Increment_1(FCPU_Z80& CPU)
{
	++CPU.Registers.PC;
}

// nop
void _00(FCPU_Z80& CPU)
{}

// ld bc, nn
void _01(FCPU_Z80& CPU)
{
	PUT_CMD([](FCPU_Z80& CPU) -> void { CPU.Cycle_MemoryReadCycle(*CPU.Registers.PC, CPU.Registers.BC.L, std::move(CMD_PC_Increment_1)); });
	PUT_CMD([](FCPU_Z80& CPU) -> void { CPU.Cycle_MemoryReadCycle(*CPU.Registers.PC, CPU.Registers.BC.H, std::move(CMD_PC_Increment_1)); });
}

// jr $
void _18(FCPU_Z80& CPU)
{
	PUT_CMD([](FCPU_Z80& CPU) -> void { CPU.Cycle_MemoryReadCycle(*CPU.Registers.PC, CPU.Registers.WZ.L, std::move(CMD_PC_Increment_1)); });
	PUT_CMD([](FCPU_Z80& CPU) -> void { CPU.Cycle_ALU_LoadWZ_AddWZ_UnloadWZ(); });
}

const CMD_FUNC FCPU_Z80::Unprefixed[256] =
{
	_00,	_01, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, _18, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};
