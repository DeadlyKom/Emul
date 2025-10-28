#include "ToolBar.h"
#include "Events.h"

namespace
{
	static const char* ThisWindowName = TEXT("Tool Bar");
}

SToolBar::SToolBar(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
{}

void SToolBar::Initialize()
{}

void SToolBar::Render()
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
