#pragma once

#include <CoreMinimal.h>

#define PUT_CMD(command)					(CPU.Registers.CP.Put(command))
#define GET_CMD()							(CPU.Registers.CP.Get())
#define EXE_CMD()							(CPU.Registers.CP.Execute(CPU))

class FCPU_Z80;

struct FCommandPipeline
{
	void Put(std::function<void(FCPU_Z80& CPU)>&& Command)
	{
		Buffer[Head] = std::move(Command);
		Head = (Head + 1) % Size;
	}
	
	std::function<void(FCPU_Z80& CPU)> Get()
	{
		size_t Index = Trail;
		Trail = (Trail + 1) % Size;
		return std::move(Buffer[Index]);
	}

	void Execute(FCPU_Z80& CPU)
	{
		Get()(CPU);
	}

	bool IsEmpty()
	{
		return Head == Trail;
	}

	size_t Head;
	size_t Trail;
	static constexpr size_t Size = 8;
	std::function<void(FCPU_Z80& CPU)> Buffer[Size];
};