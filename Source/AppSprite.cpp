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
#include <Window/Sprite/Events.h>

namespace
{
	static const std::string SpriteName = std::format("ZX-Sprite ver. {}.{}", SPRITE_BUILD_MAJOR, SPRITE_BUILD_MINOR);
	static const char* MenuFileName = "File";
	static const char* NewCanvasName = "New canvas";
	static const char* MenuEditName = "Edit";
	static const char* MenuQuitName = "Quit";
	static const char* MenuMetadataName = "Metadata";

	const char* ExportToName = "##ExportTo";
	static const char* AddMetaToRegionName = "##AddMetaToRegion";

	int32_t TextEditNumberCallback(ImGuiInputTextCallbackData* Data)
	{
		switch (Data->EventFlag)
		{
		case ImGuiInputTextFlags_CallbackCharFilter:
			if (Data->EventChar < '0' || Data->EventChar > '9')
			{
				return 1;
			}
			break;
		case ImGuiInputTextFlags_CallbackEdit:
			float Value = 0;
			if (strlen(Data->Buf) > 1)
			{
				Value = float(std::stoi(Data->Buf));
			}
			*(float*)Data->UserData = Value;
			break;
		}
		return 0;
	}
}

FAppSprite::FAppSprite()
	: bOpen(true)
	, CanvasCounter(0)
{
	WindowName = SpriteName.c_str();
}

void FAppSprite::Initialize()
{
	Utils::Log(LogVerbosity::Display, SpriteName.c_str());

	FAppFramework::Initialize();
	LOG("Initialize 'Sprite' application.");

	FImageBase& Images = FImageBase::Get();
	Images.Initialize(Device, DeviceContext);

	Viewer = std::make_shared<SViewerBase>(NAME_DOS_12, FrameworkConfig.WindowWidth, FrameworkConfig.WindowHeight);
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
		Viewer->SetupHotKeys();

		Viewer->SetMenuBar(std::bind(&ThisClass::Show_MenuBar, this));
		Viewer->AppendWindows({
			{ NAME_Palette,			std::make_shared<SPalette>(NAME_DOS_12, "##Layout_Palette")},
			{ NAME_ToolBar,			std::make_shared<SToolBar>(NAME_DOS_12, "##Layout_ToolBar")},
			{ NAME_StatusBar,		std::make_shared<SStatusBar>(NAME_DOS_12, "##Layout_StatusBar")},
			{ NAME_SpriteList,		std::make_shared<SSpriteList>(NAME_DOS_12, "##Layout_SpriteList")},
			//{ NAME_SpriteMetadata,	std::make_shared<SSpriteMetadata>(NAME_DOS_12)},
			}, Data, {});
		
	}

	FFonts& Fonts = FFonts::Get();
	Fonts.LoadFont(NAME_DOS_12, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 12.0f, 0);
}

void FAppSprite::LoadSettings()
{
	FSpriteSettings::Get().Load(IO::NormalizePath((FAppFramework::GetPath(EPathType::Config) / GetFilename(EFilenameType::Config)).string()));

	FSpriteSettings& SpriteSettings = FSpriteSettings::Get();
	auto ScriptFilesOptional = SpriteSettings.GetValue<std::map<std::string, std::string>>(
		{ FSpriteSettings::ScriptFilesTag, typeid(std::map<std::string, std::string>) });

	if (ScriptFilesOptional.has_value())
	{
		ScriptFiles = ScriptFilesOptional.value();
	}
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
	return bOpen && Viewer ? !Viewer->IsOpen() : true;
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

void FAppSprite::Show_MenuBar()
{
	const ImGuiID NewCanvaseID = ImGui::GetCurrentWindow()->GetID(NewCanvasName);
	const ImGuiID QuitID = ImGui::GetCurrentWindow()->GetID(MenuQuitName);
	if (ImGui::BeginMenu(MenuFileName))
	{
		if (ImGui::BeginMenu("New"))
		{
			if (ImGui::MenuItem("Canvas", "Ctrl+N"))
			{
				ImGui::OpenPopup(NewCanvaseID);
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Open", "Ctrl+O"))
		{
			std::vector<std::filesystem::directory_entry> Files;
			const std::string OldPath = Files.empty() ? "" : Files.back().path().parent_path().string();
			SFileDialog::OpenWindow(Viewer, "Select File", EDialogMode::Select,
				[this](std::filesystem::path FilePath, EDialogStage Selected) -> void
				{
					Callback_OpenFile(FilePath);
					SFileDialog::CloseWindow();
				}, OldPath, "*.*, *.png, *.scr");
		}

		if (!RecentFiles.empty())
		{
			if (ImGui::BeginMenu("Open Recent"))
			{
				for (const FRecentFiles& RecentFile : RecentFiles)
				{
					ImGui::MenuItem(RecentFile.VisibleName.c_str());
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Clear Recent Files"))
				{
					RecentFiles.clear();
				}

				ImGui::EndMenu();
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Save", "Ctrl+S")) {}
		if (ImGui::MenuItem("Save As..")) {}
		ImGui::Separator();

		if (ImGui::MenuItem(MenuQuitName, "Alt+F4"))
		{
			if (FrameworkConfig.bDontAskMeNextTime_Quit)
			{
				Quit();
			}
			else
			{
				ImGui::OpenPopup(QuitID);
			}
		}
	
		ImGui::EndMenu();
	}

	if (Viewer && ImGui::BeginMenu(MenuMetadataName))
	{
		if (ImGui::MenuItem("Show window", nullptr, false, !Viewer->IsWindowVisibility(NAME_SpriteMetadata)))
		{
			if (Viewer->GetWindows(NAME_SpriteMetadata).empty())
			{
				FNativeDataInitialize Data
				{
					.Device = Device,
					.DeviceContext = DeviceContext
				};
				Viewer->AddWindow(NAME_SpriteMetadata, std::make_shared<SSpriteMetadata>(NAME_DOS_12), Data, {});
			}
			else
			{
				Viewer->SetWindowVisibility(NAME_SpriteMetadata);
			}
		}

		ImGui::EndMenu();
	}

	if (ShowModal_WindowQuit()) {}
	else if (ShowModal_WindowNewCanvas()) {}
}

bool FAppSprite::ShowModal_WindowQuit()
{
	// always center this window when appearing
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}

	const bool bVisible = ImGui::BeginPopupModal(MenuQuitName, NULL, ImGuiWindowFlags_AlwaysAutoResize);
	if (bVisible)
	{
		ImGui::Text("All those beautiful files will be deleted.\nThis operation cannot be undone!\n\n");
		ImGui::Separator();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
		ImGui::Checkbox("Don't ask me next time", &FrameworkConfig.bDontAskMeNextTime_Quit);
		ImGui::PopStyleVar();

		if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
		{
			if (FrameworkConfig.bDontAskMeNextTime_Quit)
			{
				FSettings& Settings = FSettings::Get();
				FSettingKey Key{ ConfigTag_DontAskMeNextTime_Quit, typeid(bool) };
				Settings.SetValue(Key, FrameworkConfig.bDontAskMeNextTime_Quit);
				Settings.Save(IO::NormalizePath((FAppFramework::GetPath(EPathType::Config) / GetFilename(EFilenameType::Config)).string()));
			}

			ImGui::CloseCurrentPopup();
			Quit();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	return bVisible;
}

bool FAppSprite::ShowModal_WindowNewCanvas()
{
	// always center this window when appearing
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}

	const bool bVisible = ImGui::BeginPopupModal(NewCanvasName, NULL, ImGuiWindowFlags_AlwaysAutoResize);
	if (bVisible)
	{
		if (ImGui::IsWindowAppearing())
		{
			FRGBAImage ClipboardImage = FRGBAImage(256, 192);
			Window::ClipboardData(ClipboardImage);

			NewCanvasSize = OriginalCanvasSize = { (float)ClipboardImage.Width, (float)ClipboardImage.Height };
			sprintf(NewCanvasNameBuffer, std::format("Canvas #{}", ++CanvasCounter).c_str());
			sprintf(NewCanvasWidthBuffer, "%i\n", int32_t(NewCanvasSize.x));
			sprintf(NewCanvasHeightBuffer, "%i\n", int32_t(NewCanvasSize.y));

			bRectangularCanvas = false;
			bRoundingToMultipleEight = !ClipboardImage.IsValid();

			Log2CanvasSize = { powf(2.0f, ceilf(log2f(NewCanvasSize.x))), powf(2.0f, ceilf(log2f(NewCanvasSize.y))) };
		}

		bool bUpdateSize = false;
		const float TextWidth = ImGui::CalcTextSize("A").x;
		const float TextHeight = ImGui::GetTextLineHeightWithSpacing();
		const ImGuiInputTextFlags InputNumberTextFlags = ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackEdit;

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
		ImGui::SeparatorText("Name : ");
		ImGui::InputTextEx("##Name", NULL, NewCanvasNameBuffer, IM_ARRAYSIZE(NewCanvasNameBuffer), ImVec2(TextWidth * 20.0f, TextHeight), ImGuiInputTextFlags_None);

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
		ImGui::SeparatorText("Size : ");
		if (ImGui::Checkbox("Multiple of 8", &bRoundingToMultipleEight))
		{
			if (!bRoundingToMultipleEight)
			{
				Log2CanvasSize = { powf(2.0f, ceilf(log2f(OriginalCanvasSize.x))), powf(2.0f, ceilf(log2f(OriginalCanvasSize.y))) };
				//NewCanvasSize = ZXColorView->RectangleMarqueeRect.GetSize();
			}
			bUpdateSize = true;
		}
		if (ImGui::Checkbox("Rectangular canvas", &bRectangularCanvas))
		{
			if (!bRectangularCanvas)
			{
				Log2CanvasSize = { powf(2.0f, ceilf(log2f(OriginalCanvasSize.x))), powf(2.0f, ceilf(log2f(OriginalCanvasSize.y))) };
				//NewCanvasSize = ZXColorView->RectangleMarqueeRect.GetSize();
			}

			bUpdateSize = true;
		}
		ImGui::Dummy(ImVec2(0.0f, TextHeight * 1.0f));

		ImGui::Text("Width :");
		ImGui::SameLine(50.0f);
		if (ImGui::InputTextEx("##Width", NULL, NewCanvasWidthBuffer, IM_ARRAYSIZE(NewCanvasWidthBuffer),
			ImVec2(TextWidth * 10.0f, TextHeight),
			InputNumberTextFlags,
			&TextEditNumberCallback,
			(void*)&OriginalCanvasSize.x))
		{
			Log2CanvasSize = { powf(2.0f, ceilf(log2f(OriginalCanvasSize.x))), powf(2.0f, ceilf(log2f(OriginalCanvasSize.y))) };
			bUpdateSize = true;
		}

		ImGui::SameLine(150.0f);

		if (ImGui::SliderFloat("##FineTuningX", &NewCanvasSize.x, 8.0f, Log2CanvasSize.x, "%.0f"))
		{
			if (bRectangularCanvas)
			{
				NewCanvasSize.y = NewCanvasSize.x;
				Log2CanvasSize.y = Log2CanvasSize.x;
			}
			bUpdateSize = true;
		}

		ImGui::Text("Height :");
		ImGui::SameLine(50.0f);
		if(ImGui::InputTextEx("##Height", NULL, NewCanvasHeightBuffer, IM_ARRAYSIZE(NewCanvasHeightBuffer),
			ImVec2(TextWidth * 10.0f, TextHeight), 
			InputNumberTextFlags,
			&TextEditNumberCallback,
			(void*)&OriginalCanvasSize.y))
		{
			Log2CanvasSize = { powf(2.0f, ceilf(log2f(OriginalCanvasSize.x))), powf(2.0f, ceilf(log2f(OriginalCanvasSize.y))) };
			bUpdateSize = true;
		}

		ImGui::SameLine(150.0f);
		if (ImGui::SliderFloat("##FineTuningY", &NewCanvasSize.y, bRoundingToMultipleEight ? 8.0f : 1.0f, Log2CanvasSize.y, "%.0f"))
		{
			if (bRectangularCanvas)
			{
				NewCanvasSize.x = NewCanvasSize.y;
				Log2CanvasSize.x = Log2CanvasSize.y;
			}

			bUpdateSize = true;
		}

		if (bUpdateSize)
		{
			if (bRectangularCanvas)
			{
				const float MinSize = ImMin(NewCanvasSize.x, NewCanvasSize.y);
				NewCanvasSize = { MinSize, MinSize };
			}
			if (bRoundingToMultipleEight)
			{
				NewCanvasSize.x = ceilf(NewCanvasSize.x / 8.0f) * 8.0f;
				NewCanvasSize.y = ceilf(NewCanvasSize.y / 8.0f) * 8.0f;
			}
			sprintf(NewCanvasWidthBuffer, "%i\n", int32_t(NewCanvasSize.x));
			sprintf(NewCanvasHeightBuffer, "%i\n", int32_t(NewCanvasSize.y));
		}

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 1.0f));

		if (ImGui::ButtonEx("OK", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			FNativeDataInitialize Data
			{
				.Device = Device,
				.DeviceContext = DeviceContext
			};

			std::wstring Filename = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.from_bytes(NewCanvasNameBuffer);
			std::shared_ptr<SCanvas> NewCanvas = std::make_shared<SCanvas>(NAME_DOS_12, Filename);
			Viewer->AddWindow(EName::Canvas, NewCanvas, Data, { NewCanvasSize });

			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	return bVisible;

}

void FAppSprite::Callback_OpenFile(std::filesystem::path FilePath)
{
}
