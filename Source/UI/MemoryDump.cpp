#include "MemoryDump.h"

namespace
{
	static const char* ThisWindowName = TEXT("Memory dump");
}

SMemoryDump::SMemoryDump()
	: SWindow(ThisWindowName, true)
{}

void SMemoryDump::Initialize()
{
}

void SMemoryDump::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	ImGui::End();
}
