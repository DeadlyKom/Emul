#include "Fonts.h"

FFonts& FFonts::Get()
{
	static std::shared_ptr<FFonts> Instance(new FFonts);
	return *Instance.get();
}

ImFont* FFonts::LoadFont(FName UniqueFontName, const void* FontData, uint32_t FontDataSize, float SizePixels, uint32_t Index /*= INDEX_NONE*/)
{
	static const ImWchar FontRanges[] = { 0x0020, 0x03ff, 0 };

	ImFont* NewFont = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(FontData, FontDataSize, SizePixels, nullptr, Index >= 0 ? &FontRanges[Index] : nullptr);
	if (!NewFont)
	{
		return nullptr;
	}

	Fonts.emplace(UniqueFontName, NewFont);
	return NewFont;
}

ImFont* FFonts::GetFont(FName UniqueFontName)
{
	const std::map<FName, ImFont*>::iterator& SearchIt = Fonts.find(UniqueFontName);
	return SearchIt != Fonts.end() ? SearchIt->second : nullptr;
}
