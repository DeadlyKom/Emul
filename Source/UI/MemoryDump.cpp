#include "MemoryDump.h"

namespace
{
	static const char* ThisWindowName = TEXT("Memory dump");
}

SMemoryDump::SMemoryDump(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(FontName)
		.SetIncludeInWindows(true))
{}

void SMemoryDump::Initialize()
{
}

void SMemoryDump::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	ImGui::End();
}
