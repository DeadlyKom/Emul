#pragma once

#include <CoreMinimal.h>

class FFonts
{
public:
	static FFonts& Get();

	ImFont* LoadFont(FName UniqueFontName, const void* FontData, uint32_t FontDataSize, float SizePixels, uint32_t Index = INDEX_NONE);
	ImFont* GetFont(FName UniqueFontName);

private:
	std::map<FName, ImFont*> Fonts;
};