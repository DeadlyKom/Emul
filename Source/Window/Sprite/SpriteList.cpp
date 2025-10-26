#include "SpriteList.h"

namespace
{
	static const char* ThisWindowName = TEXT("Sprite List");
}

SSpriteList::SSpriteList(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
{}

void SSpriteList::Initialize()
{}

void SSpriteList::Render()
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
