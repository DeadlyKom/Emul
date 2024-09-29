#include "CallStack.h"

namespace
{
	static const char* ThisWindowName = TEXT("Call stack");
}

SCallStack::SCallStack(EFont::Type _FontName)
	: SWindow(ThisWindowName, _FontName, true)
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
