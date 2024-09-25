#include "Name.h"

namespace
{
	static const char* EmptyName = "";
	static constexpr uint32_t FNameMaxBlockBits = 13;
	static constexpr uint32_t FNameMaxBlocks = 1 << FNameMaxBlockBits;

	static std::atomic_int Counter = 0;
	static std::mutex NameMutex;

	static uint32_t CurrentBlock = 0;
	static char* Blocks[FNameMaxBlocks] = {};
}

const char* GetName(uint32_t Index)
{
	return Index < CurrentBlock ? Blocks[Index] : EmptyName;
}

uint32_t AddName(const char* _Name)
{
	NameMutex.lock();

	const size_t LengthString = std::strlen(_Name);
	char* StrPtr = new char[LengthString + 1];
	const uint32_t Index = CurrentBlock;

	std::memcpy(StrPtr, _Name, LengthString);
	*(StrPtr + LengthString) = '\0';
	Blocks[CurrentBlock++] = StrPtr;

	NameMutex.unlock();

	return Index;
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
			Blocks[i] = nullptr;
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
	if (CurrentBlock < FNameMaxBlocks)
	{
		ID = FindName(_Name);
		if (ID == INDEX_NONE)
		{
			ID = AddName(_Name);
		}
	}
	else
	{
		ID = INDEX_NONE;
	}
	Counter++;
}

FName::FName(const std::string& _StrName)
{
	if (CurrentBlock < FNameMaxBlocks)
	{
		
		ID = FindName(_StrName.c_str());
		if (ID == INDEX_NONE)
		{
			ID = AddName(_StrName.c_str());
		}
	}
	else
	{
		ID = INDEX_NONE;
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
