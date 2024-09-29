#include "Fonts.h"

FFonts& FFonts::Get()
{
	static std::shared_ptr<FFonts> Instance(new FFonts);
	return *Instance.get();
}

ImFont* FFonts::LoadFont(EFont::Type FontName, const void* FontData, uint32_t FontDataSize, float SizePixels, uint32_t Index /*= INDEX_NONE*/)
{
	static const ImWchar FontRanges[] = { 0x0020, 0x03ff, 0 };

	ImFont* NewFont = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(FontData, FontDataSize, SizePixels, nullptr, Index >= 0 ? &FontRanges[Index] : nullptr);
	if (!NewFont)
	{
		return nullptr;
	}

	Fonts.emplace(FontName, NewFont);
	return NewFont;
}

ImFont* FFonts::GetFont(EFont::Type FontName)
{
	return Fonts.contains(FontName) ? Fonts[FontName] : nullptr;
}

void FFonts::SetSize(EFont::Type FontName, int32_t SizePixels)
{
	ImFont* FoundFont = GetFont(FontName);
	if (FoundFont == nullptr)
	{
		return;
	}

	FoundFont->Scale = (float)SizePixels / FoundFont->ConfigData->SizePixels;
}
