#include "AppSprite.h"
#include "Fonts/Dos2000_ru_en.cpp"
#include "Window/Sprite/Definition.h"

#include "Window/Sprite/Canvas.h"
#include "Window/Sprite/Palette.h"
#include "Window/Sprite/ToolBar.h"
#include "Window/Sprite/StatusBar.h"
#include "Window/Sprite/SpriteList.h"

namespace
{
	static const std::string SpriteName = std::format(TEXT("ZX-Sprite ver. {}.{}"), SPRITE_BUILD_MAJOR, SPRITE_BUILD_MINOR);
	static const char* MenuEditName = TEXT("Edit");

	const char* ConvertName = "##ConvertTo";
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

	FImageBase& Images = FImageBase::Get();
	Images.Initialize(Device, DeviceContext);

	Viewer = std::make_shared<SViewerBase>(NAME_DOS_12, WindowWidth, WindowHeight);
	if (Viewer)
	{
		Viewer->SetMenuBar(std::bind(&ThisClass::Show_MenuBar, this));
		Viewer->AppendWindows({
			{ NAME_Canvas,		std::make_shared<SCanvas>(NAME_DOS_12)},
			{ NAME_Palette,		std::make_shared<SPalette>(NAME_DOS_12)},
			{ NAME_ToolBar,		std::make_shared<SToolBar>(NAME_DOS_12)},
			{ NAME_StatusBar,	std::make_shared<SStatusBar>(NAME_DOS_12)},
			{ NAME_SpriteList,	std::make_shared<SSpriteList>(NAME_DOS_12)},
			});

		FNativeDataInitialize Data
		{
			.Device = Device,
			.DeviceContext = DeviceContext
		};
		Viewer->NativeInitialize(Data);
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

void FAppSprite::LoadIniSettings()
{
	const char* DefaultIni = R"(
[Window][Palette]
Pos=0,18
Size=216,66
Collapsed=0
DockId=0x00000001,0

[Window][WindowOverViewport_11111111]
Pos=0,18
Size=1008,711
Collapsed=0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][Status Bar]
Pos=0,677
Size=769,52
Collapsed=0
DockId=0x00000004,0

[Window][Canvas]
Pos=0,86
Size=769,589
Collapsed=0
DockId=0x00000006,0

[Window][Tool Bar]
Pos=218,18
Size=551,66
Collapsed=0
DockId=0x00000003,0

[Window][Sprite List]
Pos=771,18
Size=237,711
Collapsed=0
DockId=0x00000008,0

[Docking][Data]
DockSpace         ID=0x7C6B3D9B Window=0xA87D555D Pos=456,205 Size=1008,711 Split=X
  DockNode        ID=0x00000007 Parent=0x7C6B3D9B SizeRef=769,711 Split=Y
    DockNode      ID=0x00000002 Parent=0x00000007 SizeRef=1008,945 Split=Y
      DockNode    ID=0x00000005 Parent=0x00000002 SizeRef=1008,66 Split=X Selected=0xDFE559BD
        DockNode  ID=0x00000001 Parent=0x00000005 SizeRef=216,91 HiddenTabBar=1 Selected=0x7E84447F
        DockNode  ID=0x00000003 Parent=0x00000005 SizeRef=551,91 HiddenTabBar=1 Selected=0xDFE559BD
      DockNode    ID=0x00000006 Parent=0x00000002 SizeRef=1008,877 CentralNode=1 HiddenTabBar=1 Selected=0x429E880E
    DockNode      ID=0x00000004 Parent=0x00000007 SizeRef=1008,52 HiddenTabBar=1 Selected=0x1604805B
  DockNode        ID=0x00000008 Parent=0x7C6B3D9B SizeRef=237,711 HiddenTabBar=1 Selected=0x7FB5F2E9
)";
	ImGui::LoadIniSettingsFromMemory(DefaultIni);
}

void FAppSprite::Show_MenuBar()
{
	const ImGuiID ConvertID = ImGui::GetCurrentWindow()->GetID(ConvertName);
	if (ImGui::BeginMenu(MenuEditName))
	{
		if (ImGui::MenuItem("Conver"))
		{
			ImGui::OpenPopup(ConvertID);
		}

		ImGui::EndMenu();
	}
	auto a = ImGui::IsPopupOpen(ConvertID, ImGuiPopupFlags_None);
	if (ImGui::BeginPopupModal(ConvertName, 0))
	{
		bool b = false;
		ImGui::Checkbox("Grid", &b);
		ImGui::EndPopup();
	}
}
