#include "Name.h"

namespace
{
	static const char* EmptyName = "";
	static constexpr uint32_t FNameMaxBlockBits = 13;
	static constexpr uint32_t FNameMaxBlocks = 1 << FNameMaxBlockBits;

	static uint32_t Counter = 0;
	static uint32_t CurrentBlock = 0;
	static char* Blocks[FNameMaxBlocks] = {};
}

const char* GetName(uint32_t Index)
{
	return Index < CurrentBlock ? Blocks[Index] : EmptyName;
}

uint32_t AddName(const char* _Name)
{
	const size_t LengthString = sizeof(_Name) / sizeof(*_Name) ;
	char* StrPtr = new char[LengthString];
	std::memcpy(StrPtr, _Name, LengthString);
	Blocks[CurrentBlock++] = StrPtr;
	return CurrentBlock - 1;
}

uint32_t FindName(const char* _Name)
{
	for (uint32_t i = 0; i < CurrentBlock; ++i)
	{
		char* StrPtr = Blocks[i];
		if (std::strcmp(StrPtr, _Name) == 0)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void Release()
{
	if (--Counter > 0)
	{
		return;
	}

	// remove all
	for (uint32_t i = 0; i < CurrentBlock; ++i)
	{
		char* StrPtr = Blocks[i];
		if (StrPtr)
		{
			delete[] StrPtr;
			StrPtr = nullptr;
		}
	}
}

FName::FName()
	: ID(INDEX_NONE)
{
	Counter++;
}

FName::FName(const char* _Name)
{
	ID = FindName(_Name);
	if (ID == INDEX_NONE)
	{
		ID = AddName(_Name);
	}
	Counter++;
}

FName::FName(const FName& Other)
	: ID(Other.ID)
{
	Counter++;
}

FName::~FName()
{
	Release();
}

std::string FName::ToString() const
{
	return GetName(ID);
}
