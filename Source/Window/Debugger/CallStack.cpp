#include "CallStack.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Call stack";
}

SCallStack::SCallStack(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
{}

void SCallStack::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen);
	ImGui::End();
}
