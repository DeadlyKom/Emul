#pragma once

#include <CoreMinimal.h>

#define NAME_None -1

class FName
{
public:
	FName(const char* _Name);

	bool operator==(const FName& Other) const
	{
		return ID == Other.ID;
	}
	bool operator!=(const FName& Other) const
	{
		return ID != Other.ID;
	}
	bool operator<(const FName& Other) const noexcept
	{
		// logic here
		return ID < Other.ID;
	}

private:
	uint64_t ID;
};
