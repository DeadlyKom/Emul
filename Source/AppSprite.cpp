#include "AppSprite.h"
#include <Utils/IO.h>
#include "Fonts/Dos2000_ru_en.cpp"
#include "Window/Sprite/Definition.h"
#include <Settings/SpriteSettings.h>

#include <Window/Sprite/Canvas.h>
#include <Window/Sprite/Palette.h>
#include <Window/Sprite/ToolBar.h>
#include <Window/Sprite/StatusBar.h>
#include <Window/Sprite/SpriteList.h>
#include <Window/Common/FileDialog.h>
#include <Window/Sprite/SpriteMetadata.h>

namespace
{
	static const std::string SpriteName = std::format(TEXT("ZX-Sprite ver. {}.{}"), SPRITE_BUILD_MAJOR, SPRITE_BUILD_MINOR);
	static const char* MenuEditName = TEXT("Edit");
	static const char* MenuMetadataName = TEXT("Metadata");

	const char* ExportToName = "##ExportTo";
	static const char* AddMetaToRegionName = "##AddMetaToRegion";

	static const char* SettingsFilename = "Settings.cfg";
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
			{ NAME_Palette,			std::make_shared<SPalette>(NAME_DOS_12, "##Layout_Palette")},
			{ NAME_ToolBar,			std::make_shared<SToolBar>(NAME_DOS_12, "##Layout_ToolBar")},
			{ NAME_StatusBar,		std::make_shared<SStatusBar>(NAME_DOS_12, "##Layout_StatusBar")},
			{ NAME_SpriteList,		std::make_shared<SSpriteList>(NAME_DOS_12, "##Layout_SpriteList")},
			{ NAME_SpriteMetadata,	std::make_shared<SSpriteMetadata>(NAME_DOS_12)},
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
	std::shared_ptr<SCanvas> NewCanvas = std::make_shared<SCanvas>(NAME_DOS_12, Filename, FilePath);
	Viewer->AddWindow(EName::Canvas, NewCanvas, Data, FilePath);
}

void FAppSprite::LoadSettings()
{
	FSpriteSettings::Get().Load(IO::NormalizePath((FAppFramework::GetPath(EPathType::Config) / SettingsFilename).string()));
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

	if (Viewer && ImGui::BeginMenu(MenuMetadataName))
	{
		if (ImGui::MenuItem("Show window", nullptr, false, !Viewer->IsWindowVisibility(NAME_SpriteMetadata)))
		{
			Viewer->SetWindowVisibility(NAME_SpriteMetadata);
		}

		if (ImGui::MenuItem("ToDo"))
		{
		}

		ImGui::EndMenu();
	}

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
