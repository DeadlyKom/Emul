#include "AppSprite.h"
#include <Utils/IO.h>
#include <Utils/Aseprite/Format.h>
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
#include <Version.h>

namespace
{
	static const std::string SpriteName = std::format(TEXT("ZX-Sprite ver. {}.{}.{} ({})"), VER_MAJOR, VER_MINOR, VER_BUILD, VER_REVISION);

	static const char* MenuFileName = "File";
	static const char* NewCanvasName = "New canvas";
	static const char* MenuEditName = "Edit";
	static const char* MenuQuitName = "Quit";
	static const char* GridSettingsName = "Grid Settings";
	static const char* MenuMetadataName = "Metadata";

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
		Viewer->Initialize({ Layout });
		Viewer->SetupHotKeys();
		SetupHotKeys();

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
	Viewer->GetEventSystem().Subscribe<FEvent_Canvas>(
		this,
		[this](const FEvent_Canvas& Event)
		{
			if (Event.Tag == FEventTag::RequestCanvasViewFlagsTag)
			{
				FEvent_Canvas Event;
				Event.Tag = FEventTag::CanvasViewFlagsTag;
				Event.CanvasName = {};
				Event.ViewFlags = ViewFlags;
				Viewer->GetEventSystem().Publish(Event);
			}
		});
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

EImageFormat FAppSprite::SupportImageFormat(const std::filesystem::path& FilePath)
{
	if (FilePath.extension() == L".png")
	{
		return EImageFormat::PNG;
	}
	else if (FilePath.extension() == L".aseprite")
	{
		return EImageFormat::Aseprite;
	}
	else
	{
		return EImageFormat::None;
	}
}

void FAppSprite::Import_Image(const std::shared_ptr<SViewerBase>& Viewer, const std::filesystem::path& FilePath, EImageFormat ImageFormat)
{
	std::shared_ptr<SWindow> FoundWindow;
	for (std::shared_ptr<SWindow>& Window : Viewer->GetWindows(NAME_Canvas))
	{
		std::shared_ptr<SCanvas> Canvas = dynamic_pointer_cast<SCanvas>(Window);
		if (!Canvas || Canvas->GetSourcePathFile() != FilePath)
		{
			continue;
		}
		FoundWindow = Window;
		break;
	}

	if (!FoundWindow)
	{
		FNativeDataInitialize Data = Viewer->GetNativeDataInitialize();
		if (ImageFormat == EImageFormat::PNG)
		{
			std::wstring Filename = FilePath.filename().wstring();
			std::shared_ptr<SCanvas> NewCanvas = std::make_shared<SCanvas>(NAME_DOS_12, Filename, FilePath);
			Viewer->AddWindow(EName::Canvas, NewCanvas, Data, { FilePath, ImageFormat });
		}
		else if (ImageFormat == EImageFormat::Aseprite)
		{
			AsepriteFormat::FSprite Sprite;
			if (!AsepriteFormat::Load(FilePath, Sprite))
			{
				LOG_ERROR("[{}]\t Failed to parse the Aseprite format.", (__FUNCTION__));
				return;
			}

			std::filesystem::path Path = FilePath.parent_path();
			std::filesystem::path Filename = FilePath.stem();
			for (int32_t Index = 0; Index < Sprite.Frames.size(); ++Index)
			{
				const std::vector<uint8_t>& ImageData = Sprite.Frames[Index];

				std::wstring FrameFilename = std::format(L"{}_frame_{}", Filename.wstring(), Index);
				AsepriteFormat::FFrame Frame;
				Frame.Width = Sprite.Width;
				Frame.Height = Sprite.Height;
				Frame.TransparentColor = Sprite.TransparentColor;
				Frame.Image = ImageData;
				Frame.Name = FrameFilename;
				Frame.Path = Path;

				std::shared_ptr<SCanvas> NewCanvas = std::make_shared<SCanvas>(NAME_DOS_12, FrameFilename, FilePath);
				Viewer->AddWindow(EName::Canvas, NewCanvas, Data, { Frame, EImageFormat::Aseprite_Frame, Index});
			}
		}
		else
		{
			LOG_ERROR("[{}]\t Unsupported format : {}", (__FUNCTION__), (int32_t)ImageFormat);
		}
	}
	else
	{
		FoundWindow->SetOpen(true);
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
		Input_HotKeys();
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
	if (FilePath.extension() == L".json")
	{
		return Import_JSON(FilePath);
	}
	else
	{
		EImageFormat ImageFormat = SupportImageFormat(FilePath);
		switch (ImageFormat)
		{
		case EImageFormat::PNG:			return Import_Image(Viewer, FilePath, EImageFormat::PNG);
		case EImageFormat::Aseprite:	return Import_Image(Viewer, FilePath, EImageFormat::Aseprite);
		}
	}
	LOG_ERROR("[{}]\t Format not supported.", (__FUNCTION__));
}

void FAppSprite::SetupHotKeys()
{
	Hotkeys =
	{
		// global
		{ ImGuiMod_Ctrl | ImGuiKey_W,					ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteAlways,	std::bind(&ThisClass::Imput_Close,	this)	},	// (ctrl + S)
		{ ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_W,	ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteAlways,	std::bind(&ThisClass::Imput_CloseAll,	this)},	// (ctrl + S)
	};

}

void FAppSprite::Show_MenuBar()
{
	const ImGuiID NewCanvaseID = ImGui::GetCurrentWindow()->GetID(NewCanvasName);
	const ImGuiID QuitID = ImGui::GetCurrentWindow()->GetID(MenuQuitName);
	const ImGuiID GridSettingsID = ImGui::GetCurrentWindow()->GetID(GridSettingsName);

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
		if (ImGui::MenuItem("Save As..", "Ctrl+Shift+S")) {}
		if (ImGui::MenuItem("Close", "Ctrl+W"))
		{
			Imput_Close();
		}
		if (ImGui::MenuItem("Close All", "Ctrl+Shift+W"))
		{
			Imput_CloseAll();
		}

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

	if (ImGui::BeginMenu("View"))
	{
		if (ImGui::BeginMenu("Show"))
		{
			bool bUpdateOptions = false;
			if (ImGui::MenuItem("Transparent", NULL, ViewFlags.bTransparentMask))
			{
				ViewFlags.bTransparentMask = !ViewFlags.bTransparentMask;
				bUpdateOptions = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Grid", NULL, ViewFlags.bGrid))
			{
				ViewFlags.bGrid = !ViewFlags.bGrid;
				bUpdateOptions = true;
			}
			if (ImGui::MenuItem("Pixel Grid", NULL, ViewFlags.bPixelGrid))
			{
				ViewFlags.bPixelGrid = !ViewFlags.bPixelGrid;
				bUpdateOptions = true;
			}
			if (ImGui::MenuItem("Attribute Grid", NULL, ViewFlags.bAttributeGrid))
			{
				ViewFlags.bAttributeGrid = !ViewFlags.bAttributeGrid;
				bUpdateOptions = true;
			}
			if (ImGui::MenuItem("Alpha Checkerboard", NULL, ViewFlags.bAlphaCheckerboardGrid))
			{
				ViewFlags.bAlphaCheckerboardGrid = !ViewFlags.bAlphaCheckerboardGrid;
				bUpdateOptions = true;
			}
			if (ViewFlags.bAlphaCheckerboardGrid)
			{
				if (ImGui::ColorEdit4("MyColor##3", (float*)&ViewFlags.TransparentColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha))
				{
					bUpdateOptions = true;
				}
			}
			ImGui::EndMenu();

			if (bUpdateOptions)
			{
				FEvent_Canvas Event;
				Event.Tag = FEventTag::CanvasViewFlagsTag;
				Event.CanvasName = {};
				Event.ViewFlags = ViewFlags;
				Viewer->GetEventSystem().Publish(Event);
			}
		}
		ImGui::Separator();

		if (ImGui::MenuItem("Grid Settings"))
		{
			ImGui::OpenPopup(GridSettingsID);
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
	else if (ShowModal_WindowgGridSettings()) {}
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

bool FAppSprite::ShowModal_WindowgGridSettings()
{
	// always center this window when appearing
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}

	const bool bVisible = ImGui::BeginPopupModal(GridSettingsName, NULL, ImGuiWindowFlags_AlwaysAutoResize);
	if (bVisible)
	{
		bool bUpdateOptions = false;
		if (ImGui::IsWindowAppearing())
		{
			bTmpGrid = ViewFlags.bGrid;
			TmpGridSettingSize = ViewFlags.GridSettingSize;
			TmpGridSettingOffset = ViewFlags.GridSettingOffset;

			ViewFlags.bGrid = true;

			sprintf(GridSettingsWidthBuffer, "%i\n", int(TmpGridSettingSize.x));
			sprintf(GridSettingsHeightBuffer, "%i\n", int(TmpGridSettingSize.y));
			sprintf(GridSettingsOffsetXBuffer, "%i\n", int(TmpGridSettingOffset.x));
			sprintf(GridSettingsOffsetYBuffer, "%i\n", int(TmpGridSettingOffset.y));
		}

		const float TextWidth = ImGui::CalcTextSize("A").x;
		const float TextHeight = ImGui::GetTextLineHeightWithSpacing();

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
		ImGui::Text("Size :");
		ImGui::Separator();

		const ImGuiInputTextFlags InputNumberTextFlags = ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackEdit;
		bUpdateOptions |= ImGui::InputTextEx("Width ", NULL, GridSettingsWidthBuffer, IM_ARRAYSIZE(GridSettingsWidthBuffer), ImVec2(TextWidth * 10.0f, TextHeight), InputNumberTextFlags, &TextEditNumberCallback, (void*)&ViewFlags.GridSettingSize.x);
		bUpdateOptions |= ImGui::InputTextEx("Height ", NULL, GridSettingsHeightBuffer, IM_ARRAYSIZE(GridSettingsHeightBuffer), ImVec2(TextWidth * 10.0f, TextHeight), InputNumberTextFlags, &TextEditNumberCallback, (void*)&ViewFlags.GridSettingSize.y);

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 1.0f));
		ImGui::Text("Offset :");
		ImGui::Separator();

		ViewFlags.GridSettingOffset = ImClamp(ViewFlags.GridSettingOffset, ImVec2(0.0f, 0.0f), ImMax(ViewFlags.GridSettingSize - ImVec2(1.0f, 1.0f), ImVec2(0.0f, 0.0f)));
		bUpdateOptions |= ImGui::SliderFloat("X ", &ViewFlags.GridSettingOffset.x, 0, ImClamp(ViewFlags.GridSettingSize.x, 0.0f, ViewFlags.GridSettingSize.x - 1.0f), "%.0f");
		bUpdateOptions |= ImGui::SliderFloat("Y ", &ViewFlags.GridSettingOffset.y, 0, ImClamp(ViewFlags.GridSettingSize.y, 0.0f, ViewFlags.GridSettingSize.y - 1.0f), "%.0f");
		ImGui::Dummy(ImVec2(0.0f, TextHeight * 1.0f));

		ImGui::Separator();

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 1.0f));

		if (ImGui::ButtonEx("OK", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			ViewFlags.bGrid = bTmpGrid;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			ViewFlags.GridSettingSize = TmpGridSettingSize;
			ViewFlags.GridSettingOffset = TmpGridSettingOffset;
			ViewFlags.bGrid = bTmpGrid;

			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();

		if (bUpdateOptions)
		{
			FEvent_Canvas Event;
			Event.Tag = FEventTag::CanvasViewFlagsTag;
			Event.CanvasName = {};
			Event.ViewFlags = ViewFlags;
			Viewer->GetEventSystem().Publish(Event);
		}
	}
	return bVisible;
}

void  FAppSprite::Input_HotKeys()
{
	Shortcut::Handler(Hotkeys);
}

void FAppSprite::Imput_Close()
{
	for (std::shared_ptr<SWindow>& Window : Viewer->GetWindows(NAME_Canvas))
	{
		if (Window->IsFocus())
		{
			Viewer->RemoveWindow(NAME_Canvas, Window);
			break;
		}
	}
}

void FAppSprite::Imput_CloseAll()
{
	for (std::shared_ptr<SWindow>& Window : Viewer->GetWindows(NAME_Canvas))
	{
		Viewer->RemoveWindow(NAME_Canvas, Window);
	}
}

void FAppSprite::Import_JSON(const std::filesystem::path& FilePath)
{
	if (Viewer->GetWindows(NAME_SpriteList).empty())
	{
		FNativeDataInitialize Data
		{
			.Device = Device,
			.DeviceContext = DeviceContext
		};
		Viewer->AddWindow(NAME_SpriteList, std::make_shared<SSpriteMetadata>(NAME_DOS_12, "##Layout_SpriteList"), Data, {});
	}
	else
	{
		Viewer->SetWindowVisibility(NAME_SpriteList);
	}

	std::vector<std::shared_ptr<SWindow>> SpriteListWindows = Viewer->GetWindows(NAME_SpriteList);
	if (!SpriteListWindows.empty())
	{
		FEvent_ImportJSON Event;
		{
			Event.FilePath = FilePath;
		}
		Viewer->GetEventSystem().Publish(Event);
	}
}

void FAppSprite::Callback_OpenFile(std::filesystem::path FilePath)
{
}
