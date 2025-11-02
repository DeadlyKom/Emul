#include "AppSprite.h"
#include "Fonts/Dos2000_ru_en.cpp"
#include "Window/Sprite/Definition.h"
#include <Settings/SpriteSettings.h>

#include <Window/Sprite/Canvas.h>
#include <Window/Sprite/Palette.h>
#include <Window/Sprite/ToolBar.h>
#include <Window/Sprite/StatusBar.h>
#include <Window/Sprite/SpriteList.h>
#include <Window/Common/FileDialog.h>

namespace
{
	static const std::string SpriteName = std::format(TEXT("ZX-Sprite ver. {}.{}"), SPRITE_BUILD_MAJOR, SPRITE_BUILD_MINOR);
	static const char* MenuEditName = TEXT("Edit");

	const char* ExportToName = "##ExportTo";
}

FAppSprite::FAppSprite()
{
	WindowName = SpriteName.c_str();
}

void FAppSprite::Initialize()
{
	Utils::Log(LogVerbosity::Display, SpriteName.c_str());

	FAppFramework::Initialize();
	LOG("Initialize 'Sprite' application.");
	LoadIniSettings();
	LoadSettings();

	FImageBase& Images = FImageBase::Get();
	Images.Initialize(Device, DeviceContext);

	Viewer = std::make_shared<SViewerBase>(NAME_DOS_12, WindowWidth, WindowHeight);
	if (Viewer)
	{
		FNativeDataInitialize Data
		{
			.Device = Device,
			.DeviceContext = DeviceContext
		};

		FDockSlot Layout =
		{	"##Root", 1.0f, ImGuiDir_None,
			{
				{	"##Layout_SpriteList", 0.2f, ImGuiDir_Left,
					{
						{},
						{	"##Layout_Palette",  0.1f, ImGuiDir_Up,
							{
								{},
								{	"##Layout_StatusBar", 0.08f, ImGuiDir_Down, 
									{
										{	"##Layout_ToolBar", 0.06f, ImGuiDir_Right,
											{
												{	"##Layout_Canvas", 1.0f, ImGuiDir_None, {} },
												{}
											}
										},
										{}
									}
								}
							}
						}
					}}
			}};

		Viewer->NativeInitialize(Data);
		Viewer->Initialize(Layout);

		Viewer->SetMenuBar(std::bind(&ThisClass::Show_MenuBar, this));
		Viewer->AppendWindows({
			{ NAME_Palette,		std::make_shared<SPalette>(NAME_DOS_12, "##Layout_Palette")},
			{ NAME_ToolBar,		std::make_shared<SToolBar>(NAME_DOS_12, "##Layout_ToolBar")},
			{ NAME_StatusBar,	std::make_shared<SStatusBar>(NAME_DOS_12, "##Layout_StatusBar")},
			{ NAME_SpriteList,	std::make_shared<SSpriteList>(NAME_DOS_12, "##Layout_SpriteList")},
			}, Data, {});
		
	}

	FFonts& Fonts = FFonts::Get();
	Fonts.LoadFont(NAME_DOS_12, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 12.0f, 0);
}

void FAppSprite::Shutdown()
{
	if (Viewer)
	{
		Viewer->Destroy();
		Viewer.reset();
	}

	FFonts::Get().Reset();
	LOG("Shutdown 'Sprite' application.");
	FAppFramework::Shutdown();
}

void FAppSprite::Tick(float DeltaTime)
{
	if (Viewer)
	{
		Viewer->ReleaseUnnecessaryWindows();
	}

	FAppFramework::Tick(DeltaTime);

	if (Viewer)
	{
		Viewer->Tick(DeltaTime);
	}
}

void FAppSprite::Render()
{
	ImGui::PushFont(FFonts::Get().GetFont(NAME_DOS_12));

	FAppFramework::Render();

	if (Viewer)
	{
		Viewer->Render();
	}

	ImGui::PopFont();
}

bool FAppSprite::IsOver()
{
	return Viewer ? !Viewer->IsOpen() : true;
}

void FAppSprite::DragAndDropFile(const std::filesystem::path& FilePath)
{
	FNativeDataInitialize Data
	{
		.Device = Device,
		.DeviceContext = DeviceContext
	};

	std::wstring Filename = FilePath.filename().wstring();
	std::shared_ptr<SCanvas> NewCanvas = std::make_shared<SCanvas>(NAME_DOS_12, Filename);
	Viewer->AddWindow(EName::Canvas, NewCanvas, Data, FilePath);
}

void FAppSprite::LoadIniSettings()
{
//	const char* DefaultIni = R"(
//[Window][Palette]
//Pos=235,26
//Size=765,72
//Collapsed=0
//DockId=0x00000003,0
//
//[Window][WindowOverViewport_11111111]
//Pos=0,18
//Size=1008,711
//Collapsed=0
//
//[Window][Debug##Default]
//Pos=60,60
//Size=400,400
//Collapsed=0
//
//[Window][Status Bar]
//Pos=235,669
//Size=765,52
//Collapsed=0
//DockId=0x00000008,0
//
//[Window][Canvas]
//Pos=228,93
//Size=728,582
//Collapsed=0
//DockId=0x7C6B3D9B,0
//
//[Window][Tool Bar]
//Pos=932,100
//Size=68,567
//Collapsed=0
//DockId=0x00000006,0
//
//[Window][Sprite List]
//Pos=8,26
//Size=225,695
//Collapsed=0
//DockId=0x00000001,0
//
//[Window][ViewerBase]
//Pos=0,0
//Size=1008,729
//Collapsed=0
//
//[Docking][Data]
//DockSpace         ID=0x7C6B3D9B Pos=456,205 Size=1008,711 CentralNode=1 HiddenTabBar=1
//DockSpace         ID=0xD2E0236E Window=0x8C6D4AF9 Pos=464,213 Size=992,695 Split=X Selected=0x9CD3E80B
//  DockNode        ID=0x00000001 Parent=0xD2E0236E SizeRef=225,695 Selected=0x7FB5F2E9
//  DockNode        ID=0x00000002 Parent=0xD2E0236E SizeRef=695,695 Split=Y Selected=0x9CD3E80B
//    DockNode      ID=0x00000003 Parent=0x00000002 SizeRef=765,72 Selected=0x7E84447F
//    DockNode      ID=0x00000004 Parent=0x00000002 SizeRef=765,621 Split=Y Selected=0x9CD3E80B
//      DockNode    ID=0x00000007 Parent=0x00000004 SizeRef=713,567 Split=X Selected=0x9CD3E80B
//        DockNode  ID=0x00000005 Parent=0x00000007 SizeRef=695,567 CentralNode=1 Selected=0x211984C5
//        DockNode  ID=0x00000006 Parent=0x00000007 SizeRef=68,567 Selected=0xDFE559BD
//      DockNode    ID=0x00000008 Parent=0x00000004 SizeRef=713,52 Selected=0x1604805B
//)";
//	ImGui::LoadIniSettingsFromMemory(DefaultIni);
}

void FAppSprite::LoadSettings()
{
	std::string Filename = "Settings.cfg";
	FSpriteSettings::Get().Load((std::filesystem::current_path() / Filename).string());
}

void FAppSprite::Show_MenuBar()
{
	const ImGuiID ConvertID = ImGui::GetCurrentWindow()->GetID(ExportToName);
	if (ImGui::BeginMenu(MenuEditName))
	{
		if (ImGui::MenuItem("Export"))
		{
			ImGui::OpenPopup(ConvertID);
		}
	
		ImGui::EndMenu();
	}
	auto a = ImGui::IsPopupOpen(ConvertID, ImGuiPopupFlags_None);
	if (ImGui::BeginPopupModal(ExportToName, 0))
	{
		if (ImGui::Button("Cancel", ImVec2(0.0f, 0.0f)))
		{
			std::vector<std::filesystem::directory_entry> Files;
			const std::string OldPath = Files.empty() ? "" : Files.back().path().parent_path().string();
			SFileDialog::OpenWindow(Viewer, "Select File", EDialogMode::Select,
				[this](std::filesystem::path FilePath, EDialogStage Selected) -> void
				{
					//OpenFile_Callback(FilePath);
					SFileDialog::CloseWindow();
				}, OldPath, "*.*, *.png, *.scr");

			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
