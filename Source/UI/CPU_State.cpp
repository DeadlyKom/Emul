#include "CPU_State.h"

namespace
{
	static const char* CPU_StateName = TEXT("CPU State");
}

SCPU_State::SCPU_State()
	: SWindow(CPU_StateName, true)
{}

void SCPU_State::Initialize()
{
}

void SCPU_State::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(CPU_StateName, &bOpen);
	ImGui::End();
}
