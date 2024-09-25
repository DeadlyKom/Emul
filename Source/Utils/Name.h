#pragma once

#include <CoreMinimal.h>

class FName
{
public:
	FName();
	FName(const char* _Name);
	FName(const std::string& _StrName);
	FName(const FName& Other);
	~FName();

	std::string ToString() const;

	bool operator==(const FName& Other) const
	{
		return ID == Other.ID;
	}
	bool operator!=(const FName& Other) const
	{
		return ID != Other.ID;
	}
	void operator=(const FName& Other)
	{
		ID = Other.ID;
	}
	bool operator<(const FName& Other) const noexcept
	{
		return ID < Other.ID;
	}

private:
	uint32_t ID;
};
