#include "StatusBar.h"

namespace
{
	static const char* ThisWindowName = TEXT("Status Bar");
}

SStatusBar::SStatusBar(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
{}

void SStatusBar::Initialize()
{}

void SStatusBar::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	{
		ImGui::End();
	}
}
