#include "Oscillograph.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Oscillograph";
}

SOscillograph::SOscillograph(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
{}

void SOscillograph::Initialize(const std::any& Arg)
{
}

void SOscillograph::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen);
	{
		UI::Draw_Oscillogram("Test", Oscillograph);
	}
	ImGui::End();
}
