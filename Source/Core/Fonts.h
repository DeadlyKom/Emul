#pragma once

#include <CoreMinimal.h>

class FFonts
{
public:
	static FFonts& Get();

	ImFont* LoadFont(FName FontName, const void* FontData, uint32_t FontDataSize, float SizePixels, uint32_t Index = INDEX_NONE);
	ImFont* GetFont(FName FontName);

private:
	std::map<FName, ImFont*> Fonts;
};