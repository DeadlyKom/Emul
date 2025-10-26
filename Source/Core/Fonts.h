#pragma once

#include <CoreMinimal.h>

// Define a message as an enumeration.
#define REGISTER_FONT(num,name) name = num,
namespace EFont
{
	enum Type : int32_t
	{
		// Include all the hard-coded names
		#include "Font_DefaultNames.inl"

		// Special constant for the last hard-coded name index
		MaxHardcodedIndex,
	};
}
#undef REGISTER_FONT

#define REGISTER_FONT(num,name) inline constexpr EFont::Type NAME_##name = EFont::name;
#include "Font_DefaultNames.inl"
#undef REGISTER_FONT

class FFonts
{
public:
	static FFonts& Get();

	ImFont* LoadFont(EFont::Type FontName, const void* FontData, uint32_t FontDataSize, float SizePixels, uint32_t Index = INDEX_NONE);
	ImFont* GetFont(EFont::Type FontName);
	float SetSize(EFont::Type FontName, float Scale, float Min, float Max);
	void Reset();

private:
	std::map<EFont::Type, ImFont*> Fonts;
};
