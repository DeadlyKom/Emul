#include "Disassembler.h"

namespace
{
	static const char* DisassemblerName = TEXT("Disassembler");
}

SDisassembler::SDisassembler()
	: SWindow(DisassemblerName, true)
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

	ImGui::Begin(DisassemblerName, &bOpen);
	ImGui::End();
}
