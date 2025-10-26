#include "Oscillograph.h"

namespace
{
	static const char* ThisWindowName = TEXT("Oscillograph");
}

SOscillograph::SOscillograph(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
{}

void SOscillograph::Initialize()
{
}

void SOscillograph::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	{
		UI::Draw_Oscillogram("Test", Oscillograph);
	}
	ImGui::End();
}
