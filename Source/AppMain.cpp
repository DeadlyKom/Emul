#include "AppMain.h"
#include <Core/Fonts.h>
#include <Core/Settings.h>
#include <Utils/IO.h>

#include "Fonts/Dos2000_ru_en.cpp"

namespace
{
	const char* AppMainName = "Selection Window";
	const char* SelectionWindowName = "Selection Window";
	const char* ApplicationNames[] = { "Debugger", "Sprite" };
}

FAppMain::FAppMain()
	: bOpen(false)
	, bDontAskMeNextTime(false)
	, ItemSelectedIndex(INDEX_NONE)
{
	WindowName = AppMainName;
	bEnabledResize = false;
}

void FAppMain::Initialize()
{
	FAppFramework::Initialize();
	LOG("Initialize 'Main' application.");

	FrameworkCallback.Secection = EApplication::None;
	for (size_t i = 0; i < std::size(ApplicationNames); ++i)
	{
		if (FrameworkConfig.Application == ApplicationNames[i])
		{
			FrameworkCallback.Secection = static_cast<EApplication::Type>(i);
			break;
		}
	}

	if (FrameworkCallback.Secection != EApplication::None)
	{
		bOpen = true;
		return;
	}
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

void FAppMain::LoadSettings()
{
	FSettings& Settings = FSettings::Get();
	Settings.Load(IO::NormalizePath((FAppFramework::GetPath(EPathType::Config) / GetFilename(EFilenameType::Config)).string()));

	auto LogOptional = Settings.GetValue<bool>({ ConfigTag_Log, typeid(bool) });
	if (LogOptional.has_value())
	{
		FrameworkConfig.bLog = LogOptional.value();
		LOG("[*] enable logging.");
	}
	auto ResolutionOptional = Settings.GetValue<std::string>({ ConfigTag_Resolution, typeid(std::string) });
	if (ResolutionOptional.has_value())
	{
		std::string_view Resolution = *ResolutionOptional;
		size_t Pos = Resolution.find('x');
		if (Pos != std::string_view::npos)
		{
			int width = 0, height = 0;

			std::from_chars(Resolution.data(), Resolution.data() + Pos, width);
			std::from_chars(Resolution.data() + Pos + 1, Resolution.data() + Resolution.size(), height);

			FrameworkConfig.WindowWidth = width;
			FrameworkConfig.WindowHeight = height;
		}
		LOG("[*] set the resolution {}x{}.", FrameworkConfig.WindowWidth, FrameworkConfig.WindowHeight);
	}
	auto FullscreenOptional = Settings.GetValue<bool>({ ConfigTag_Fullscreen, typeid(bool) });
	if (FullscreenOptional.has_value())
	{
		FrameworkConfig.bFullscreen = FullscreenOptional.value();
		if (FrameworkConfig.bFullscreen)
		{
			FrameworkConfig.WindowWidth = ScreenWidth;
			FrameworkConfig.WindowHeight = ScreenHeight;
		}
		LOG("[*] set resolution to fullscreen.");
	}
	auto ApplicationOptional = Settings.GetValue<std::string>({ ConfigTag_Application, typeid(std::string) });
	if (ApplicationOptional.has_value())
	{
		FrameworkConfig.Application = ApplicationOptional.value();
		LOG("[*] set application: {}.", FrameworkConfig.Application);
	}
	auto DontAskMeNextTime_Quit_Optional = Settings.GetValue<bool>({ ConfigTag_DontAskMeNextTime_Quit, typeid(bool) });
	if (DontAskMeNextTime_Quit_Optional.has_value())
	{
		FrameworkConfig.bDontAskMeNextTime_Quit = DontAskMeNextTime_Quit_Optional.value();
		if (FrameworkConfig.bDontAskMeNextTime_Quit)
		{
			LOG("[*] don't ask me next time - 'Quit'.");
		}
	}
}

std::string_view FAppMain::GetDefaultConfig()
{
	static std::string DefaultConfig = R"(
{
    "Resolution": "1024x768",
    "bLog": true,
    "bDontAskMeNextTime_Quit": false,
    "bFullscreen": false,
    "Application": "None",
    "ScriptFiles": {
        "ASCII": "",
        "ATTR box": "",
        "ATTR stencil": "",
        "Column cut": "",
        "Cut screen no ATTR": "",
        "Hex v1.0": "Saved/Config/Hex.py",
        "Load": "",
        "OR XOR": "",
        "OR XOR ATTR": "",
        "OR XOR mirror": "",
        "OR XOR one mask": "",
        "Tile": "",
        "Tile Z": "",
        "Tile Z ATTR": "",
        "Tile no flip": "",
        "Tile no flip ATTR": "",
        "no ATTR box": ""
    }
}
)";

	return DefaultConfig;
}

void FAppMain::ShowSelectionWindow()
{
	// always center this window when appearing
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}

	{
		ImGui::Begin("Helper", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text(TEXT("Application"));

		const float TextWidth = ImGui::CalcTextSize("A").x;
		const float TextHeight = ImGui::GetTextLineHeightWithSpacing();
		ImGui::BeginGroup(); 
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
		ImGui::Checkbox("Don't ask me next time", &bDontAskMeNextTime);
		ImGui::EndGroup();

		if (ImGui::Button("Ok", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			FrameworkCallback.Secection = ItemSelectedIndex;
			FrameworkConfig.Application = ApplicationNames[ItemSelectedIndex];
			bOpen = true;

			if (bDontAskMeNextTime)
			{
				FSettings& Settings = FSettings::Get();
				FSettingKey Key{ ConfigTag_Application, typeid(std::string) };
				Settings.SetValue(Key, FrameworkConfig.Application);
				Settings.Save(IO::NormalizePath((FAppFramework::GetPath(EPathType::Config) / GetFilename(EFilenameType::Config)).string()));
			}
		}
		ImGui::End();
	}
}
