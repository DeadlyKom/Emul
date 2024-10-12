#pragma once

#include <CoreMinimal.h>

#define PUT_PIPELINE(type, command)	(CPU.Registers.##type.Put(command))
#define GET_PIPELINE(type)			(CPU.Registers.##type.Get())
#define EXE_PIPELINE(type)			(CPU.Registers.##type.Execute(CPU))

class FCPU_Z80;

struct FPipeline
{
	FPipeline()
		: Head(0)
		, Trail(0)
	{}

	void Put(std::function<void(FCPU_Z80& CPU)>&& Command)
	{
		Buffer[Head] = std::move(Command);
		Head = (Head + 1) % Size;
	}
	
	std::function<void(FCPU_Z80& CPU)> Get()
	{
		assert(Trail != Head);
		size_t Index = Trail;
		Trail = (Trail + 1) % Size;
		return std::exchange(Buffer[Index], nullptr);
	}

	void Execute(FCPU_Z80& CPU)
	{
		Get()(CPU);
	}

	bool IsEmpty() const
	{
		return Head == Trail;
	}

	size_t Head;
	size_t Trail;
	static constexpr size_t Size = 8;
	std::function<void(FCPU_Z80& CPU)> Buffer[Size];
};