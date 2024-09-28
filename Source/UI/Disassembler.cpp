#include "Disassembler.h"

namespace
{
	static const char* ThisWindowName = TEXT("Disassembler");
}

SDisassembler::SDisassembler()
	: SWindow(ThisWindowName, true)
{}

void SDisassembler::Initialize()
{
}

void SDisassembler::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	ImGui::End();
}
