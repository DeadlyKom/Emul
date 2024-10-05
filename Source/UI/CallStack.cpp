#include "CallStack.h"

namespace
{
	static const char* ThisWindowName = TEXT("Call stack");
}

SCallStack::SCallStack(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(FontName)
		.SetIncludeInWindows(true))
{}

void SCallStack::Initialize()
{
}

void SCallStack::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	ImGui::End();
}
