#include "AppMain.h"
#include "Core/Fonts.h"

#include "Fonts/Dos2000_ru_en.cpp"

namespace
{
	const char* AppMainName = "Selection Window";
	const char* SelectionWindowName = "Selection Window";
	const char* ApplicationNames[] = { "Debugger", "Sprite" };

}

FAppMain::FAppMain()
	: bOpen(false)
	, ItemSelectedIndex(INDEX_NONE)
{
	WindowName = AppMainName;
	bEnabledResize = false;
}

void FAppMain::Initialize()
{
	FAppFramework::Initialize();
	LOG("Initialize 'Main' application.");

	LoadIniSettings();

	FFonts& Fonts = FFonts::Get();
	Fonts.LoadFont(NAME_DOS_12, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 12.0f, 0);
}

void FAppMain::Shutdown()
{
	FFonts::Get().Reset();
	LOG("Shutdown 'Main' application.");
	FAppFramework::Shutdown();
}

void FAppMain::Render()
{
	ImGui::PushFont(FFonts::Get().GetFont(NAME_DOS_12));
	ShowSelectionWindow();
	ImGui::PopFont();
}

bool FAppMain::IsOver()
{
	return bOpen;
}

void FAppMain::LoadIniSettings()
{
	const char* DefaultIni = R"(
[Window][Selection Window]
ViewportPos=473,650
ViewportId=0x412E93F9
Size=1008,729
Collapsed=0

[Window][Helper]
Pos=0,0
Size=1008,729
Collapsed=0
)";
	ImGui::LoadIniSettingsFromMemory(DefaultIni);
}

void FAppMain::ShowSelectionWindow()
{
	// Always center this window when appearing
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	{
		ImGui::Begin("Helper", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text(TEXT("Application"));

		const float TextWidth = ImGui::CalcTextSize("A").x;
		const float TextHeight = ImGui::GetTextLineHeightWithSpacing();
		if (ImGui::BeginListBox("##SelectionApplication", ImVec2(250, 150)))
		{
			for (int32_t i = 0; i < IM_ARRAYSIZE(ApplicationNames); ++i)
			{
				const bool bIsSelected = (ItemSelectedIndex == i);
				if (ImGui::Selectable(ApplicationNames[i], bIsSelected))
				{
					ItemSelectedIndex = i;
				}
			}
			ImGui::EndListBox();
		}

		if (ImGui::Button("Ok", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			FrameworkCallback.Secection = ItemSelectedIndex;
			bOpen = true;
		}
		ImGui::End();
	}
}
