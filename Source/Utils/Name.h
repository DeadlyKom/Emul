#pragma once

#include <CoreMinimal.h>

// Define a message as an enumeration.
#define REGISTER_NAME(num,name) name = num,
namespace EName
{
	enum Type : int32_t
	{
		// Include all the hard-coded names
		#include "Name_DefaultNames.inl"

		// Special constant for the last hard-coded name index
		MaxHardcodedIndex,
	};
}
#undef REGISTER_NAME

#define REGISTER_NAME(num,name) inline constexpr EName::Type NAME_##name = EName::name;
#include "Name_DefaultNames.inl"
#undef REGISTER_NAME

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
