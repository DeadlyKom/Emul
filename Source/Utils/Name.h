#pragma once

#include <CoreMinimal.h>

// Define a message as an enumeration.
#define REGISTER_NAME(num,name) name = num,
enum class EName : int32_t
{
	// Include all the hard-coded names
	REGISTER_NAME(-1, None)
	// Special constant for the last hard-coded name index
	MaxHardcodedNameIndex,
};
#undef REGISTER_NAME

// Define aliases for the old-style EName enum syntax
#define REGISTER_NAME(num,name) inline constexpr EName NAME_##name = EName::name;
REGISTER_NAME(-1, None)
#undef REGISTER_NAME

class FName
{
public:
	FName();
	FName(const char* _Name);
	FName(const FName& Other);
	virtual ~FName();

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
