#include "AppSprite.h"
#include <Utils/IO.h>
#include <Utils/Aseprite/Format.h>
#include "Fonts/Dos2000_ru_en.cpp"
#include "Window/Sprite/Definition.h"
#include <Settings/SpriteSettings.h>

#include <Window/Sprite/Canvas.h>
#include <Window/Sprite/Palette.h>
#include <Window/Sprite/ToolBar.h>
#include <Window/Sprite/Timeline.h>
#include <Window/Sprite/StatusBar.h>
#include <Window/Sprite/SpriteList.h>
#include <Window/Common/FileDialog.h>
#include <Window/Sprite/SpriteMetadata.h>
#include <Window/Sprite/Events.h>
#include <Version.h>

namespace
{
	static const std::string SpriteName = std::format(TEXT("ZX-Sprite ver. {}.{}.{} ({})"), VER_MAJOR, VER_MINOR, VER_BUILD, VER_REVISION);

	static const char* Menu_FileName = "File";
	static const char* Menu_File_NewName = "New";
	static const char* Menu_File_New_CanvasName = "Canvas";
	static const char* Menu_File_OpenName = "Open";
	static const char* Menu_File_OpenRecentName = "Open recent";
	static const char* Menu_File_OpenRecent_ClearRecentFilesName = "Clear recent files";
	static const char* Menu_File_SaveName = "Save";
	static const char* Menu_File_SaveAsName = "Save as..";
	static const char* Menu_File_CloseName = "Close";
	static const char* Menu_File_CloseAllName = "Close all";
	static const char* Menu_File_ExportName = "Export";
	static const char* Menu_File_Export_CodeGenerationName = "Code generation";
	static const char* Menu_File_QuitName = "Quit";
	static const char* MenuEditName = "Edit";

	static const char* Menu_ViewName = "View";
	static const char* Menu_View_ShowName = "Show";
	static const char* Menu_View_GridSettingsName = "Grid settings";
	static const char* Menu_View_Show_TransparentName = "Transparent";
	static const char* Menu_View_Show_GridName = "Grid";
	static const char* Menu_View_Show_PixelGridName = "Pixel grid";
	static const char* Menu_View_Show_AttributeGridName = "Attribute grid";
	static const char* Menu_View_Show_AlphaCheckerboardName = "Alpha checkerboard";
	static const char* Menu_View_FrameModeName = "Frame mode";
	static const char* Menu_View_FrameMode_NoneName = "None";
	static const char* Menu_View_FrameMode_DifferenceName = "Difference forward";
	static const char* Menu_View_FrameMode_ReverseDifferenceName = "Difference reverse";

	static const char* Menu_FrameName = "Frame";

	static const char* Menu_MetadataName = "Metadata";
	static const char* Menu_Metadata_ShowWindowName = "Show window";


	static const char* Modal_MenuQuitName = "Quit";
	static const char* Modal_NewCanvasName = "New canvas";
	static const char* Modal_GridSettingsName = "Grid settings";
	static const char* Modal_CodeGenerationName = "Code generation";
	static const char* Modal_CodeGenerationProgressName = "Code generation progress";

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

	std::wstring MakeFrameOutputFileName(const std::wstring& FileName, int32_t FrameIndex, bool bOpcodeOutput = false)
	{
		std::filesystem::path FileNamePath(FileName);
		FileNamePath.replace_filename(std::format(L"{} ({}){}{}", FileNamePath.stem().wstring(), FrameIndex, bOpcodeOutput ? L" opcodes" : L"", FileNamePath.extension().wstring()));
		FileNamePath.replace_extension(bOpcodeOutput ? ".bin" : ".asm");
		return FileNamePath.wstring();
	}

	void ApplyCodeGenerationOutputModeToPath(std::filesystem::path& OutputPath, bool bOpcodeOutput)
	{
		if (!OutputPath.has_extension())
		{
			OutputPath.replace_extension(bOpcodeOutput ? ".bin" : ".asm");
		}
		else if (bOpcodeOutput && (OutputPath.extension() == L".asm" || OutputPath.extension() == L".txt"))
		{
			OutputPath.replace_extension(".bin");
		}
		else if (!bOpcodeOutput && OutputPath.extension() == L".bin")
		{
			OutputPath.replace_extension(".asm");
		}

		if (bOpcodeOutput)
		{
			const std::wstring Stem = OutputPath.stem().wstring();
			if (!Stem.ends_with(L" opcodes"))
			{
				OutputPath.replace_filename(Stem + L" opcodes" + OutputPath.extension().wstring());
			}
		}
		else
		{
			const std::wstring Stem = OutputPath.stem().wstring();
			const std::wstring OpcodeSuffix = L" opcodes";
			if (Stem.size() >= OpcodeSuffix.size() && Stem.ends_with(OpcodeSuffix))
			{
				OutputPath.replace_filename(Stem.substr(0, Stem.size() - OpcodeSuffix.size()) + OutputPath.extension().wstring());
			}
		}
	}

	void DrawReadOnlyMultilineText(const char* Label, const std::string& Text, const ImVec2& Size)
	{
		char EmptyText[] = "";
		char* Buffer = Text.empty() ? EmptyText : const_cast<char*>(Text.c_str());
		const size_t BufferSize = Text.empty() ? sizeof(EmptyText) : Text.size() + 1;
		ImGui::InputTextMultiline(
			Label,
			Buffer,
			BufferSize,
			Size,
			ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoUndoRedo);
	}

}

FAppSprite::FAppSprite()
	: bOpen(true)
	, FrameDifferenceDirection(EFrameMode::Difference)
	, bRectangularCanvas(false)
	, bRoundingToMultipleEight(false)
	, CanvasCounter(0)
	, NewCanvasNameBuffer{}
	, NewCanvasWidthBuffer{}
	, NewCanvasHeightBuffer{}
	, OriginalCanvasSize(0.0f, 0.0f)
	, NewCanvasSize(0.0f, 0.0f)
	, Log2CanvasSize(0.0f, 0.0f)
	, bTmpGrid(false)
	, GridSettingsWidthBuffer{}
	, GridSettingsHeightBuffer{}
	, GridSettingsOffsetXBuffer{}
	, GridSettingsOffsetYBuffer{}
	, TmpGridSettingSize(0.0f, 0.0f)
	, TmpGridSettingOffset(0.0f, 0.0f)
	, bCodeGenerationGenerateOpcode(false)
	, bCodeGenerationOpen(false)
	, bCodeGenerationShowCode(false)
	, bCodeGenerationApplyWindowSize(false)
	, bCodeGenerationPreviewValid(false)
	, bCodeGenerationCodeWindowSizeInitialized(false)
	, bCodeGenerationGenerationInProgress(false)
	, bCodeGenerationProgressModalOpen(false)
	, bCodeGenerationProgressShouldClose(false)
	, bCodeGenerationLogScrollToBottom(false)
	, bCodeGenerationCancelRequested(false)
	, CodeGenerationProgressCurrent(0)
	, CodeGenerationProgressTotal(0)
	, ExportCounter(0)
	, CodeGenerationWindowHeight(0.0f)
	, NewOutputFileNameBuffer{}
	, CodeGenerationLabelNameBuffer{}
	, CodeGenerationBaseWindowSize(0.0f, 0.0f)
	, CodeGenerationCodeWindowSize(0.0f, 0.0f)
{
	WindowName = SpriteName.c_str();
	Utils::CopyToBuffer(CodeGenerationLabelNameBuffer, sizeof(CodeGenerationLabelNameBuffer), CodeGenerator::MakeFrameLabelName(0));
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

		const FDockSlot Layout =
			{"##Root", 1.0f, ImGuiDir_None, {
				{"##Layout_StatusBar", 0.05f, ImGuiDir_Down, {
					{"##Layout_SpriteList", 0.2f, ImGuiDir_Left, {
						{},
						{"##Layout_Timeline", 0.1f, ImGuiDir_Down, {
							{"##Layout_ToolBar", 0.06f, ImGuiDir_Right, {
								{"##Layout_Palette",  0.15f, ImGuiDir_Up, {
									{},
									{"##Layout_Canvas", 1.0f, ImGuiDir_None, {
									}}
								}}
							}}
						}}
					}}
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
			{ NAME_Timeline,		std::make_shared<STimeline>(NAME_DOS_12, "##Layout_Timeline")},
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
	Viewer->GetEventSystem().Subscribe<FEvent_AppSprite>(
		this,
		[this](const FEvent_AppSprite& Event)
		{
			std::shared_ptr<SCanvas> Event_Canvas = std::dynamic_pointer_cast<SCanvas>(Event.Canvas);
			if (Event.Tag == FEventTag::NotificationAddCanvasTag)
			{
				auto It = std::find(ActiveCanvas.begin(), ActiveCanvas.end(), Event_Canvas);
				if (It == ActiveCanvas.end())
				{
					ActiveCanvas.push_back(Event_Canvas);
				}
			}
			else if (Event.Tag == FEventTag::NotificationRemoveCanvasTag)
			{
				auto It = std::find(ActiveCanvas.begin(), ActiveCanvas.end(), Event_Canvas);
				if (It != ActiveCanvas.end())
				{
					ActiveCanvas.erase(It);
				}
			}
			Viewer->SetWindowVisibility(EName::Timeline, HasCanvasWithTimeline());
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

bool FAppSprite::Import_Image(const std::shared_ptr<SViewerBase>& Viewer, const std::filesystem::path& FilePath, EImageFormat ImageFormat)
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
			std::shared_ptr<AsepriteFormat::FSprite> Sprite = std::make_shared<AsepriteFormat::FSprite>();
			if (!AsepriteFormat::Load(FilePath, *Sprite.get()))
			{
				LOG_ERROR("[{}]\t Failed to parse the Aseprite format.", (__FUNCTION__));
				return false;
			}

			std::filesystem::path Filename = FilePath.stem();
			std::shared_ptr<SCanvas> NewCanvas = std::make_shared<SCanvas>(NAME_DOS_12, Filename, FilePath);
			Viewer->AddWindow(EName::Canvas, NewCanvas, Data, { EImageFormat::Aseprite, Sprite });
		}
		else
		{
			LOG_ERROR("[{}]\t Unsupported format : {}", (__FUNCTION__), (int32_t)ImageFormat);
			return false;
		}
	}
	else
	{
		FoundWindow->SetOpen(true);
	}

	return true;
}

void FAppSprite::Shutdown()
{
	bCodeGenerationCancelRequested.store(true, std::memory_order_relaxed);
	if (CodeGenerationWorker.joinable())
	{
		CodeGenerationWorker.join();
	}

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

	PollCodeGenerationPreviewJob();
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

	ShowModal_CodeGenerationProgress();

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
		OpenFile(FilePath);
		return;
	}
	LOG_ERROR("[{}]\t Format not supported.", (__FUNCTION__));
}

void FAppSprite::SetupHotKeys()
{
	Hotkeys =
	{
		// global
		{ ImGuiKey_KeypadMultiply, ImGuiInputFlags_RouteGlobal, std::bind(&ThisClass::Imput_ToggleFrameDirection, this) }, // (Num *)
		{ ImGuiMod_Ctrl | ImGuiKey_W,					ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteGlobal,	std::bind(&ThisClass::Imput_Close,							this)	},	// (ctrl + S)
		{ ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_W,	ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteGlobal,	std::bind(&ThisClass::Imput_CloseAll,						this)	},	// (ctrl + S)
		{ ImGuiKey_KeypadDivide,						ImGuiInputFlags_RouteGlobal,	std::bind(&ThisClass::Imput_ToggleFrameMode,				this)	},	// (Num /)
	};
}

void FAppSprite::Show_MenuBar()
{
	const ImGuiID QuitID = ImGui::GetCurrentWindow()->GetID(Modal_MenuQuitName);
	const ImGuiID NewCanvaseID = ImGui::GetCurrentWindow()->GetID(Modal_NewCanvasName);
	const ImGuiID GridSettingsID = ImGui::GetCurrentWindow()->GetID(Modal_GridSettingsName);
	const ImGuiID CodeGenerationID = ImGui::GetCurrentWindow()->GetID(Modal_CodeGenerationName);

	if (ImGui::BeginMenu(Menu_FileName))
	{
		if (ImGui::BeginMenu(Menu_File_NewName))
		{
			if (ImGui::MenuItem(Menu_File_New_CanvasName, "Ctrl+N"))
			{
				ImGui::OpenPopup(NewCanvaseID);
			}
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem(Menu_File_OpenName, "Ctrl+O"))
		{
			const std::string OldPath = RecentFiles.empty() ? "" : RecentFiles.back().parent_path().string();
			SFileDialog::OpenWindow(Viewer, "Select File", EDialogMode::Select,
				[this](std::filesystem::path FilePath, EDialogStage Selected) -> void
				{
					if (Callback_OpenFile(FilePath))
					{
						RecentFiles.push_back(FilePath);
					}
					SFileDialog::CloseWindow();
				}, OldPath, "*.*, *.aseprite, *.png, *.scr");
		}
		if (!RecentFiles.empty())
		{
			if (ImGui::BeginMenu(Menu_File_OpenRecentName))
			{
				for (const std::filesystem::path& RecentFile : RecentFiles)
				{
					ImGui::MenuItem(RecentFile.filename().string().c_str());
				}

				ImGui::Separator();
				if (ImGui::MenuItem(Menu_File_OpenRecent_ClearRecentFilesName))
				{
					RecentFiles.clear();
				}

				ImGui::EndMenu();
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem(Menu_File_SaveName, "Ctrl+S")) {}
		if (ImGui::MenuItem(Menu_File_SaveAsName, "Ctrl+Shift+S")) {}
		if (ImGui::MenuItem(Menu_File_CloseName, "Ctrl+W"))
		{
			Imput_Close();
		}
		if (ImGui::MenuItem(Menu_File_CloseAllName, "Ctrl+Shift+W"))
		{
			Imput_CloseAll();
		}

		ImGui::Separator();

		// export
		{
			const bool bHasExport = ActiveCanvas.size() > 0;
			if (bHasExport && ImGui::BeginMenu(Menu_File_ExportName))
			{
				if (ImGui::MenuItem(Menu_File_Export_CodeGenerationName))
				{
					bCodeGenerationOpen = true;
				}
				ImGui::EndMenu();
			}

			if (bHasExport)
			{
				ImGui::Separator();
			}
		}

		if (ImGui::MenuItem(Menu_File_QuitName, "Alt+F4"))
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
	if (ImGui::BeginMenu(Menu_ViewName))
	{
		if (ImGui::BeginMenu(Menu_View_ShowName))
		{
			bool bUpdateOptions = false;
			if (ImGui::MenuItem(Menu_View_Show_TransparentName, NULL, ViewFlags.bTransparentMask))
			{
				ViewFlags.bTransparentMask = !ViewFlags.bTransparentMask;
				bUpdateOptions = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem(Menu_View_Show_GridName, NULL, ViewFlags.bGrid))
			{
				ViewFlags.bGrid = !ViewFlags.bGrid;
				bUpdateOptions = true;
			}
			if (ImGui::MenuItem(Menu_View_Show_PixelGridName, NULL, ViewFlags.bPixelGrid))
			{
				ViewFlags.bPixelGrid = !ViewFlags.bPixelGrid;
				bUpdateOptions = true;
			}
			if (ImGui::MenuItem(Menu_View_Show_AttributeGridName, NULL, ViewFlags.bAttributeGrid))
			{
				ViewFlags.bAttributeGrid = !ViewFlags.bAttributeGrid;
				bUpdateOptions = true;
			}
			if (ImGui::MenuItem(Menu_View_Show_AlphaCheckerboardName, NULL, ViewFlags.bAlphaCheckerboardGrid))
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
		if (ImGui::BeginMenu(Menu_View_FrameModeName, HasCanvasWithTimeline()))
		{
			bool bUpdateOptions = false;
			if (ImGui::MenuItem(Menu_View_FrameMode_NoneName, "Num /", ViewFlags.FrameMode == EFrameMode::None))
			{
				ViewFlags.FrameMode = EFrameMode::None;
				bUpdateOptions = true;
			}
			if (ImGui::MenuItem(Menu_View_FrameMode_DifferenceName, nullptr, ViewFlags.FrameMode == EFrameMode::Difference))
			{
				FrameDifferenceDirection = EFrameMode::Difference;
				ViewFlags.FrameMode = EFrameMode::Difference;
				bUpdateOptions = true;
			}
			if (ImGui::MenuItem(Menu_View_FrameMode_ReverseDifferenceName, "Num *", ViewFlags.FrameMode == EFrameMode::ReverseDifference))
			{
				FrameDifferenceDirection = EFrameMode::ReverseDifference;
				ViewFlags.FrameMode = EFrameMode::ReverseDifference;
				bUpdateOptions = true;
			}
			ImGui::EndMenu();

			if (bUpdateOptions)
			{
				FEvent_Canvas Canvas_Event(FEventTag::CanvasViewFlagsTag);
				{
					Canvas_Event.CanvasName = {};
					Canvas_Event.ViewFlags = ViewFlags;
					Viewer->GetEventSystem().Publish(Canvas_Event);
				}
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem(Menu_View_GridSettingsName))
		{
			ImGui::OpenPopup(GridSettingsID);
		}

		ImGui::EndMenu();
	}
	if (Viewer && ImGui::BeginMenu(Menu_MetadataName))
	{
		if (ImGui::MenuItem(Menu_Metadata_ShowWindowName, nullptr, false, !Viewer->IsWindowVisibility(NAME_SpriteMetadata)))
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
	else if (bCodeGenerationOpen) { Show_WindowgCodeGeneration(); }
}

bool FAppSprite::ShowModal_WindowQuit()
{
	// always center this window when appearing
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}

	const bool bVisible = ImGui::BeginPopupModal(Modal_MenuQuitName, NULL, ImGuiWindowFlags_AlwaysAutoResize);
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

	const bool bVisible = ImGui::BeginPopupModal(Modal_NewCanvasName, NULL, ImGuiWindowFlags_AlwaysAutoResize);
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

	const bool bVisible = ImGui::BeginPopupModal(Modal_GridSettingsName, NULL, ImGuiWindowFlags_AlwaysAutoResize);
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

bool FAppSprite::Show_WindowgCodeGeneration()
{
	// always center this window when appearing
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}

	{
		ImGuiViewport* Viewport = ImGui::GetMainViewport();
		const ImGuiStyle& Style = ImGui::GetStyle();
		const float PreTextWidth = ImGui::CalcTextSize("A").x;
		const float PreTextHeight = ImGui::GetTextLineHeightWithSpacing();
		const float PreLeftWidth = PreTextWidth * 72.0f;
		const float PreCodeWidth = PreTextWidth * 80.0f;
		const ImVec2 BaseMinSize(PreLeftWidth + Style.WindowPadding.x * 2.0f, PreTextHeight * 52.0f);
		const ImVec2 CodeMinSize(BaseMinSize.x + Style.ItemSpacing.x + PreCodeWidth, BaseMinSize.y);

		if (CodeGenerationBaseWindowSize.x <= 0.0f || CodeGenerationBaseWindowSize.y <= 0.0f)
		{
			CodeGenerationBaseWindowSize = ImVec2(BaseMinSize.x, Viewport->WorkSize.y * 0.9f);
		}
		if (CodeGenerationWindowHeight <= 0.0f)
		{
			CodeGenerationWindowHeight = CodeGenerationBaseWindowSize.y;
		}
		if (CodeGenerationCodeWindowSize.x <= 0.0f || CodeGenerationCodeWindowSize.y <= 0.0f)
		{
			CodeGenerationCodeWindowSize = ImVec2(CodeGenerationBaseWindowSize.x + Style.ItemSpacing.x + PreCodeWidth, CodeGenerationBaseWindowSize.y);
		}
		CodeGenerationBaseWindowSize.y = CodeGenerationWindowHeight;
		CodeGenerationCodeWindowSize.y = CodeGenerationWindowHeight;

		ImGui::SetNextWindowSizeConstraints(
			bCodeGenerationShowCode ? CodeMinSize : ImVec2(BaseMinSize.x, BaseMinSize.y),
			bCodeGenerationShowCode ? ImVec2(FLT_MAX, FLT_MAX) : ImVec2(BaseMinSize.x, FLT_MAX));
		ImGui::SetNextWindowSize(
			bCodeGenerationShowCode ? CodeGenerationCodeWindowSize : CodeGenerationBaseWindowSize,
			bCodeGenerationApplyWindowSize ? ImGuiCond_Always : ImGuiCond_Appearing);
		bCodeGenerationApplyWindowSize = false;
	}

	const bool bVisible = ImGui::Begin(Modal_CodeGenerationName, &bCodeGenerationOpen, ImGuiWindowFlags_NoCollapse);
	if (bVisible)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			bCodeGenerationOpen = false;
			//ImGui::CloseCurrentPopup();
			ImGui::End();
			return false;
		}

		const bool bShowCodeThisFrame = bCodeGenerationShowCode;
		if (ImGui::IsWindowAppearing())
		{
			std::shared_ptr<SCanvas> Canvas = GetActiveCanvas();
			std::wstring CanvasName = Canvas ? Canvas->GetWindowWName() : std::format(L"Export-{:04}", ++ExportCounter);
			const int32_t FrameIndex = Canvas ? Canvas->GetSelectedFrameIndex() : 0;
			std::wstring OutputFileNameW = MakeFrameOutputFileName(CanvasName, FrameIndex, bCodeGenerationGenerateOpcode);
			std::string OutputFileNameUtf8 = Utils::Utf16ToUtf8(OutputFileNameW);
			Utils::CopyToBuffer(NewOutputFileNameBuffer, sizeof(NewOutputFileNameBuffer), OutputFileNameUtf8);
			Utils::CopyToBuffer(CodeGenerationLabelNameBuffer, sizeof(CodeGenerationLabelNameBuffer), CodeGenerator::MakeFrameLabelName(FrameIndex));
			bCodeGenerationPreviewValid = false;
			CodeGenerationPreviewText.clear();
			CodeGenerationOpcodeBytes.clear();
			if (CodeGenerationLogText.empty())
			{
				CodeGenerationLogText = "Нажмите Generate для запуска.";
			}
			CodeGenerationBaseWindowSize = ImVec2(0.0f, 0.0f);
			CodeGenerationCodeWindowSize = ImVec2(0.0f, 0.0f);
			CodeGenerationWindowHeight = 0.0f;
			bCodeGenerationCodeWindowSizeInitialized = false;
		}

		const float TextWidth = ImGui::CalcTextSize("A").x;
		const float TextHeight = ImGui::GetTextLineHeightWithSpacing();
		const ImVec2 AvailableSize = ImGui::GetContentRegionAvail();
		const float LeftWidth = bShowCodeThisFrame ? TextWidth * 72.0f : AvailableSize.x;
		const float PanelHeight = AvailableSize.y;
		const float BottomButtonsHeight = TextHeight * 2.0f;
		const float OptionsHeight = ImMax(TextHeight * 8.0f, PanelHeight - BottomButtonsHeight - ImGui::GetStyle().ItemSpacing.y);
		const float ViewCodeButtonWidth = TextWidth * 14.0f;
		const float RefreshButtonWidth = TextWidth * 11.0f;
		const float InputNumberWidth = TextWidth * 14.0f;
		const float OutputFileWidth = TextWidth * 56.0f;
		bool bOptionsChanged = false;
		auto CheckboxWithTooltip = [&](const char* Label, bool* Value, const char* Tooltip)
		{
			if (ImGui::Checkbox(Label, Value))
			{
				bOptionsChanged = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(Tooltip);
				ImGui::EndTooltip();
			}
		};
		auto DrawLastItemTooltip = [](const char* Tooltip)
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(Tooltip);
				ImGui::EndTooltip();
			}
		};

		ImGui::BeginChild("##CodeGenerationOptionsPanel", ImVec2(LeftWidth, PanelHeight), false);
		ImGui::BeginChild("##CodeGenerationOptionsContent", ImVec2(0.0f, OptionsHeight), false);

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
		ImGui::SeparatorText("Output file : ");
		ImGui::InputTextEx("##Name", NULL, NewOutputFileNameBuffer, IM_ARRAYSIZE(NewOutputFileNameBuffer), ImVec2(OutputFileWidth, TextHeight), ImGuiInputTextFlags_None);

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
		ImGui::SeparatorText("Code generation options : ");
		bOptionsChanged |= ImGui::InputTextEx("Label", NULL, CodeGenerationLabelNameBuffer, IM_ARRAYSIZE(CodeGenerationLabelNameBuffer), ImVec2(TextWidth * 32.0f, TextHeight), ImGuiInputTextFlags_ReadOnly);

		if (ImGui::Button("Speed", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			CodeGenerationOptions.CycleWeight = 1000;
			CodeGenerationOptions.ByteWeight = 1;
			CodeGenerationOptions.NonLinearBeamWidth = 2;
			CodeGenerationOptions.MaxNonLinearCandidatesToEvaluatePerPass = 64;
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Скорость: планировщик почти всегда выбирает меньше тактов, даже если код станет больше.\nПоиск уже, генерация быстрее.");
		ImGui::SameLine();
		if (ImGui::Button("Balanced", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			CodeGenerationOptions.CycleWeight = 10;
			CodeGenerationOptions.ByteWeight = 1;
			CodeGenerationOptions.NonLinearBeamWidth = 4;
			CodeGenerationOptions.MaxNonLinearCandidatesToEvaluatePerPass = 96;
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Баланс: такты важнее размера, но размер кода тоже влияет на выбор.");
		ImGui::SameLine();
		if (ImGui::Button("Small", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			CodeGenerationOptions.CycleWeight = 1;
			CodeGenerationOptions.ByteWeight = 200;
			CodeGenerationOptions.NonLinearBeamWidth = 8;
			CodeGenerationOptions.MaxNonLinearCandidatesToEvaluatePerPass = 192;
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Размер: планировщик сильно штрафует каждый байт кода.\nПоиск шире, чтобы найти более короткие комбинации.");

		CodeGenerationOptions.MaxStackPairsToEnumerate = ImMax(CodeGenerationOptions.MaxStackPairsToEnumerate, 2);
		CodeGenerationOptions.CycleWeight = ImMax(CodeGenerationOptions.CycleWeight, 1LL);
		CodeGenerationOptions.ByteWeight = ImMax(CodeGenerationOptions.ByteWeight, 1LL);
		ImGui::SetNextItemWidth(InputNumberWidth);
		bOptionsChanged |= ImGui::InputInt("Max stack pairs", &CodeGenerationOptions.MaxStackPairsToEnumerate);
		DrawLastItemTooltip("Максимальная длина перебираемого стекового блока в 16-битных парах.\nБольше значение может найти длиннее PUSH-блоки, но увеличивает время поиска.");
		int32_t StackTopAddress = CodeGenerationOptions.StackTopAddress;
		ImGui::SetNextItemWidth(InputNumberWidth);
		if (ImGui::InputInt("Stack top address", &StackTopAddress))
		{
			CodeGenerationOptions.StackTopAddress = static_cast<uint16_t>(ImClamp(StackTopAddress, 0, 0xFFFF));
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Адрес временной вершины стека для PUSH-записей и CALL-трамплина.\nПри сохранении SP временно портятся 4 байта: top-2, top-1, top и top+1.");
		ImGui::SameLine();
		if (ImGui::SmallButton("#5B02"))
		{
			CodeGenerationOptions.StackTopAddress = CodeGenerator::ZX_STACK_TRAMPOLINE_TOP;
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Рекомендуемый адрес в printer buffer: используются #5B00..#5B03 сразу после экрана.");
		ImGui::SameLine();
		if (ImGui::SmallButton("#FF00"))
		{
			CodeGenerationOptions.StackTopAddress = 0xFF00;
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Старый вариант во верхней памяти: используются #FEFE..#FF01.");
		ImGui::TextUnformatted("Screen target");
		ImGui::SameLine();
		if (ImGui::RadioButton("Base #4000", CodeGenerationOptions.ScreenBaseAddress == CodeGenerator::ZX_SCREEN_BASE))
		{
			CodeGenerationOptions.ScreenBaseAddress = CodeGenerator::ZX_SCREEN_BASE;
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Генерировать запись в основной экран ZX Spectrum: #4000..#5AFF.");
		ImGui::SameLine();
		if (ImGui::RadioButton("Shadow #C000", CodeGenerationOptions.ScreenBaseAddress == CodeGenerator::ZX_SHADOW_SCREEN_BASE))
		{
			CodeGenerationOptions.ScreenBaseAddress = CodeGenerator::ZX_SHADOW_SCREEN_BASE;
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Генерировать запись в теневой экран ZX Spectrum: #C000..#DAFF.");
		CheckboxWithTooltip(
			"Generate pixels",
			&CodeGenerationOptions.GeneratePixels,
			"Учитывать bitmap-область экрана: 6144 байта #4000..#57FF или соответствующую область теневого экрана.");
		CheckboxWithTooltip(
			"Generate attributes",
			&CodeGenerationOptions.GenerateAttributes,
			"Учитывать атрибуты цвета: 768 байт #5800..#5AFF или соответствующую область теневого экрана.");
		CheckboxWithTooltip(
			"Reverse frame difference",
			&CodeGenerationOptions.ReverseFrameDifference,
			"Прямой diff записывает текущий кадр поверх предыдущего. Обратный diff записывает предыдущий кадр поверх текущего.");
		CheckboxWithTooltip(
			"Project canvas selection",
			&CodeGenerationOptions.ProjectSelection,
			"Генерировать только видимое выделение canvas и перенести его в указанную позицию экрана 256x192. Координата X выравнивается вниз до границы 8 пикселей.");
		if (!CodeGenerationOptions.ProjectSelection)
		{
			ImGui::BeginDisabled();
		}
		ImGui::SetNextItemWidth(InputNumberWidth);
		bOptionsChanged |= ImGui::InputInt("Destination X", &CodeGenerationOptions.DestinationX);
		DrawLastItemTooltip("Горизонтальная координата назначения 0..255. Bitmap ZX адресуется байтами, поэтому X выравнивается вниз до кратного 8.");
		ImGui::SetNextItemWidth(InputNumberWidth);
		bOptionsChanged |= ImGui::InputInt("Destination Y", &CodeGenerationOptions.DestinationY);
		DrawLastItemTooltip("Вертикальная координата назначения 0..191. Пиксели переносятся точно; атрибуты попадают в соответствующую строку знакомест.");
		if (!CodeGenerationOptions.ProjectSelection)
		{
			ImGui::EndDisabled();
		}
		int32_t CycleWeight = static_cast<int32_t>(CodeGenerationOptions.CycleWeight);
		ImGui::SetNextItemWidth(InputNumberWidth);
		if (ImGui::InputInt("Cycle weight", &CycleWeight))
		{
			CodeGenerationOptions.CycleWeight = ImMax(CycleWeight, 1);
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Вес одного такта Z80 в оценке кандидата.\nЧем больше значение, тем сильнее генератор предпочитает быстрый код.");
		int32_t ByteWeight = static_cast<int32_t>(CodeGenerationOptions.ByteWeight);
		ImGui::SetNextItemWidth(InputNumberWidth);
		if (ImGui::InputInt("Code byte weight", &ByteWeight))
		{
			CodeGenerationOptions.ByteWeight = ImMax(ByteWeight, 1);
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Вес одного байта машинного кода в оценке кандидата.\nЧем больше значение, тем сильнее генератор предпочитает короткий код.");
		int32_t BeamWidth = CodeGenerationOptions.NonLinearBeamWidth;
		ImGui::SetNextItemWidth(InputNumberWidth);
		if (ImGui::InputInt("Search beam width", &BeamWidth))
		{
			CodeGenerationOptions.NonLinearBeamWidth = ImClamp(BeamWidth, 1, 32);
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Ширина поиска: сколько лучших промежуточных вариантов планировщик держит одновременно.\nБольше значение может улучшить результат, но замедляет генерацию.");
		int32_t NonLinearProbeLimit = CodeGenerationOptions.MaxNonLinearCandidatesToEvaluatePerPass;
		ImGui::SetNextItemWidth(InputNumberWidth);
		if (ImGui::InputInt("Pattern probe limit", &NonLinearProbeLimit))
		{
			CodeGenerationOptions.MaxNonLinearCandidatesToEvaluatePerPass = ImClamp(NonLinearProbeLimit, 1, 1024);
			bOptionsChanged = true;
		}
		DrawLastItemTooltip("Лимит проверки нелинейных паттернов за один шаг поиска.\nЭто вертикали, стековые блоки, повторы и другие варианты, которые конкурируют с линейной записью.");
		CheckboxWithTooltip(
			"Preserve SP",
			&CodeGenerationOptions.PreserveSP,
			"Сохраняет SP вокруг стековых кандидатов, когда это возможно.\nМеняется генерируемая последовательность, а не формула оценки.");
		CheckboxWithTooltip(
			"Disable interrupts for stack writes",
			&CodeGenerationOptions.DisableInterruptsForStack,
			"Добавляет DI/EI вокруг стековых записей.\nПолезно, когда сгенерированный блок нельзя прерывать.");
		const bool bPrevGenerateOpcode = bCodeGenerationGenerateOpcode;
		CheckboxWithTooltip(
			"Generate opcode instead of text file",
			&bCodeGenerationGenerateOpcode,
			"Переключает экспорт в бинарные опкоды вместо текстового .asm файла.\nПревью остаётся читаемым Z80 asm.");
		if (bPrevGenerateOpcode != bCodeGenerationGenerateOpcode)
		{
			std::filesystem::path OutputPath(Utils::Utf8ToUtf16(NewOutputFileNameBuffer));
			ApplyCodeGenerationOutputModeToPath(OutputPath, bCodeGenerationGenerateOpcode);
			Utils::CopyToBuffer(NewOutputFileNameBuffer, sizeof(NewOutputFileNameBuffer), Utils::Utf16ToUtf8(OutputPath.wstring()));
		}

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.35f));
		ImGui::SeparatorText("Code generation approaches : ");
		CheckboxWithTooltip(
			"Byte candidates",
			&CodeGenerationOptions.EnableByteCandidates,
			"Включает одиночные байтовые записи через:\n LD A, n\n LD (nn), A\n и\n LD HL, nn\n LD (HL), n\n Стоимость выбирается через Cycle weight и Code byte weight.");
		CheckboxWithTooltip(
			"Word candidates",
			&CodeGenerationOptions.EnableWordCandidates,
			"Включает 2-байтовые записи через:\n LD HL, word\n LD (nn), HL\n Полезно, когда соседние байты образуют одинаковое 16-битное слово.");
		CheckboxWithTooltip(
			"Stack blocks",
			&CodeGenerationOptions.EnableStackBlocks,
			"Включает стековые блоки через:\n LD SP, end\n LD HL, word\n PUSH HL\n ...\n Обычно даёт меньше байт кода и хороший баланс тактов/размера.");
		CheckboxWithTooltip(
			"Repeat words",
			&CodeGenerationOptions.EnableRepeatWords,
			"Включает повторяющиеся 16-битные значения через:\n LD (addr), HL\n ...\n или\n PUSH HL\n...\n Используется для повторяющихся WORD-паттернов в dirty-области.");
		CheckboxWithTooltip(
			"Horizontal same byte: INC L",
			&CodeGenerationOptions.EnableHorizontalSameByteIncL,
			"Включает горизонтальные цепочки одинакового байта через INC L.\nСтрока остаётся на одной странице, оценка идёт той же формулой.");
		CheckboxWithTooltip(
			"Vertical candidates",
			&CodeGenerationOptions.EnableVerticalCandidates,
			"Включает нелинейные кандидаты по форме экрана:\n LD HL, addr\n LD (HL), n\n INC H\n ...\n и вертикальные повторяющиеся WORD-записи с шагом addr+#0100.");
		CheckboxWithTooltip(
			"Reverse directions",
			&CodeGenerationOptions.EnableReverseDirections,
			"Разрешает обратные проходы через DEC H и DEC L.\nПолезно, когда порядок генерации может переиспользовать указатель с другой стороны.");
		CheckboxWithTooltip(
			"Register constants: B/C/D/E",
			&CodeGenerationOptions.EnableRegisterConstants,
			"Считает частые dirty-байты, держит лучшие значения в B/C/D/E и разрешает записи через эти регистры.\nСтоимость LD r,n или LD rr,nn включается в оценку плана.");

		if (bOptionsChanged)
		{
			bCodeGenerationPreviewValid = false;
		}

		CodeGenerationOptions.MaxStackPairsToEnumerate = ImClamp(CodeGenerationOptions.MaxStackPairsToEnumerate, 2, 256);
		CodeGenerationOptions.NonLinearBeamWidth = ImClamp(CodeGenerationOptions.NonLinearBeamWidth, 1, 32);
		CodeGenerationOptions.MaxNonLinearCandidatesToEvaluatePerPass = ImClamp(CodeGenerationOptions.MaxNonLinearCandidatesToEvaluatePerPass, 1, 1024);
		CodeGenerationOptions.CycleWeight = ImMax(CodeGenerationOptions.CycleWeight, 1LL);
		CodeGenerationOptions.ByteWeight = ImMax(CodeGenerationOptions.ByteWeight, 1LL);
		CodeGenerationOptions.DestinationX = ImClamp(CodeGenerationOptions.DestinationX, 0, 255);
		CodeGenerationOptions.DestinationY = ImClamp(CodeGenerationOptions.DestinationY, 0, 191);
		if (CodeGenerationOptions.ScreenBaseAddress != CodeGenerator::ZX_SCREEN_BASE &&
			CodeGenerationOptions.ScreenBaseAddress != CodeGenerator::ZX_SHADOW_SCREEN_BASE)
		{
			CodeGenerationOptions.ScreenBaseAddress = CodeGenerator::ZX_SCREEN_BASE;
		}
		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.35f));
		const bool bGenerationInProgress = bCodeGenerationGenerationInProgress.load(std::memory_order_relaxed);
		const bool bGenerationBusy = bGenerationInProgress || bCodeGenerationProgressModalOpen || bCodeGenerationProgressShouldClose;
		const char* GenerateButtonLabel = bGenerationBusy ? "Generating..." : (bCodeGenerationPreviewValid ? "Refresh" : "Generate");
		if (!bGenerationBusy && ImGui::Button(GenerateButtonLabel, ImVec2(RefreshButtonWidth, TextHeight * 1.5f)))
		{
			StartCodeGenerationPreview();
		}

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
		ImGui::SeparatorText("Compilation log : ");
		if (ImGui::SmallButton("Clear log"))
		{
			CodeGenerationLogText.clear();
			bCodeGenerationLogScrollToBottom = false;
		}
		ImGui::BeginChild("##CodeGenerationLog", ImVec2(0.0f, ImGui::GetContentRegionAvail().y), true, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::TextUnformatted(CodeGenerationLogText.c_str());
		if (bCodeGenerationLogScrollToBottom)
		{
			ImGui::SetScrollHereY(1.0f);
			if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
			{
				bCodeGenerationLogScrollToBottom = false;
			}
		}
		ImGui::EndChild();

		ImGui::EndChild();
		ImGui::SetCursorPosY(ImMax(ImGui::GetCursorPosY(), PanelHeight - BottomButtonsHeight));

		if (!bCodeGenerationPreviewValid)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::ButtonEx("Export", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			ExportCodeGenerationPreview();
		}

		if (!bCodeGenerationPreviewValid)
		{
			ImGui::EndDisabled();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			bCodeGenerationOpen = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		ImGui::SetCursorPosX(ImMax(ImGui::GetCursorPosX(), ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ViewCodeButtonWidth));
		if (ImGui::Button(bShowCodeThisFrame ? "<<< Hide Code" : "View Code >>>", ImVec2(ViewCodeButtonWidth, TextHeight * 1.5f)))
		{
			const ImGuiStyle& Style = ImGui::GetStyle();
			ImGuiWindow* RootWindow = ImGui::GetCurrentWindow()->RootWindow;
			const ImVec2 CurrentWindowSize = RootWindow ? RootWindow->SizeFull : ImGui::GetWindowSize();
			if (bShowCodeThisFrame)
			{
				CodeGenerationBaseWindowSize = CurrentWindowSize;
				CodeGenerationWindowHeight = CurrentWindowSize.y;
				CodeGenerationBaseWindowSize.y = CodeGenerationWindowHeight;
			}
			else
			{
				const float CodeExtraWidth = TextWidth * 80.0f;
				CodeGenerationCodeWindowSize = CurrentWindowSize;
				CodeGenerationWindowHeight = CurrentWindowSize.y;
				CodeGenerationBaseWindowSize.y = CodeGenerationWindowHeight;
				if (!bCodeGenerationCodeWindowSizeInitialized)
				{
					CodeGenerationCodeWindowSize = ImVec2(CurrentWindowSize.x + Style.ItemSpacing.x + CodeExtraWidth, CodeGenerationWindowHeight);
					bCodeGenerationCodeWindowSizeInitialized = true;
				}
				else
				{
					CodeGenerationCodeWindowSize.y = CodeGenerationWindowHeight;
				}
			}
			bCodeGenerationShowCode = !bCodeGenerationShowCode;
			bCodeGenerationApplyWindowSize = true;
		}

		ImGui::EndChild();

		if (bShowCodeThisFrame)
		{
			ImGui::SameLine();
			ImGui::BeginChild("##CodeGenerationCodePanel", ImVec2(0.0f, PanelHeight), false);
			ImGui::TextUnformatted("Code :");
			ImGui::Separator();
			ImGui::BeginChild("##CodeGenerationPreviewPanel", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
			{
				if (CodeGenerationPreviewText.empty())
				{
					ImGui::TextDisabled("No generated code.");
				}
				else
				{
					DrawReadOnlyMultilineText("##CodeGenerationPreview", CodeGenerationPreviewText, ImGui::GetContentRegionAvail());
				}
			}
			ImGui::EndChild();
			ImGui::EndChild();
		}

		if (bShowCodeThisFrame)
		{
			ImGuiWindow* RootWindow = ImGui::GetCurrentWindow()->RootWindow;
			const ImVec2 CurrentWindowSize = RootWindow ? RootWindow->SizeFull : ImGui::GetWindowSize();
			const bool bUserResizing = ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
				ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
				(ImAbs(CurrentWindowSize.x - CodeGenerationCodeWindowSize.x) > 1.0f ||
				 ImAbs(CurrentWindowSize.y - CodeGenerationCodeWindowSize.y) > 1.0f);
			if (bUserResizing)
			{
				CodeGenerationCodeWindowSize = CurrentWindowSize;
				CodeGenerationWindowHeight = CurrentWindowSize.y;
			}
		}
		else
		{
			ImGuiWindow* RootWindow = ImGui::GetCurrentWindow()->RootWindow;
			const ImVec2 CurrentWindowSize = RootWindow ? RootWindow->SizeFull : ImGui::GetWindowSize();
			const bool bUserResizing = ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
				ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
				ImAbs(CurrentWindowSize.y - CodeGenerationWindowHeight) > 1.0f;
			if (bUserResizing)
			{
				CodeGenerationWindowHeight = CurrentWindowSize.y;
			}
		}
		ImGui::End();
	}
	return bVisible;
}

void FAppSprite::StartCodeGenerationPreview()
{
	if (bCodeGenerationGenerationInProgress.load(std::memory_order_relaxed))
	{
		return;
	}

	auto AppendLogLine = [this](const std::string& Line)
	{
		if (!CodeGenerationLogText.empty())
		{
			CodeGenerationLogText.push_back('\n');
		}
		CodeGenerationLogText += Line;
		bCodeGenerationLogScrollToBottom = true;
	};

	CodeGenerationJobResult = CodeGenerator::FResult();
	bCodeGenerationPreviewValid = false;
	bCodeGenerationProgressShouldClose = false;
	CodeGenerationProgressCurrent.store(0, std::memory_order_relaxed);
	CodeGenerationProgressTotal.store(100, std::memory_order_relaxed);

	AppendLogLine("Generating source code...");

	std::shared_ptr<SCanvas> Canvas = GetActiveCanvas();
	if (!Canvas)
	{
		AppendLogLine("No active canvas.");
		return;
	}

	const int32_t FrameIndex = Canvas->GetSelectedFrameIndex();
	Utils::CopyToBuffer(NewOutputFileNameBuffer, sizeof(NewOutputFileNameBuffer), Utils::Utf16ToUtf8(MakeFrameOutputFileName(Canvas->GetWindowWName(), FrameIndex, bCodeGenerationGenerateOpcode)));
	Utils::CopyToBuffer(CodeGenerationLabelNameBuffer, sizeof(CodeGenerationLabelNameBuffer), CodeGenerator::MakeFrameLabelName(FrameIndex));

	if (CodeGenerationWorker.joinable())
	{
		CodeGenerationWorker.join();
	}

	bCodeGenerationCancelRequested.store(false, std::memory_order_relaxed);
	bCodeGenerationGenerationInProgress.store(true, std::memory_order_relaxed);
	bCodeGenerationProgressModalOpen = true;

	CodeGenerator::FOptions Options = CodeGenerationOptions;
	Options.OutputOpcodes = bCodeGenerationGenerateOpcode;
	const std::string LabelName = CodeGenerationLabelNameBuffer;
	CodeGenerationWorker = std::thread(
		[this, Canvas, Options, LabelName]()
		{
			CodeGenerator::FProgressInfo Progress;
			Progress.CancelRequested = &bCodeGenerationCancelRequested;
			Progress.Current = &CodeGenerationProgressCurrent;
			Progress.Total = &CodeGenerationProgressTotal;

			CodeGenerationJobResult = Canvas->BuildCodeGenerationResult(Options, LabelName, &Progress);
			bCodeGenerationGenerationInProgress.store(false, std::memory_order_relaxed);
		});
}

void FAppSprite::RefreshCodeGenerationPreview()
{
	StartCodeGenerationPreview();
}

void FAppSprite::PollCodeGenerationPreviewJob()
{
	if (bCodeGenerationGenerationInProgress.load(std::memory_order_relaxed))
	{
		return;
	}

	if (!CodeGenerationWorker.joinable())
	{
		return;
	}

	CodeGenerationWorker.join();

	auto AppendLogLine = [this](const std::string& Line)
	{
		if (!CodeGenerationLogText.empty())
		{
			CodeGenerationLogText.push_back('\n');
		}
		CodeGenerationLogText += Line;
		bCodeGenerationLogScrollToBottom = true;
	};

	if (!CodeGenerationJobResult.bSuccess)
	{
		if (bCodeGenerationCancelRequested.load(std::memory_order_relaxed))
		{
			AppendLogLine("Cancelled.");
		}
		else
		{
			AppendLogLine(std::format("Failed: {}", CodeGenerationJobResult.Error));
		}
	}
	else
	{
		bCodeGenerationPreviewValid = true;
		CodeGenerationPreviewText = std::move(CodeGenerationJobResult.AsmCode);
		CodeGenerationOpcodeBytes = std::move(CodeGenerationJobResult.ByteCode);
		AppendLogLine(std::format("Source code size: {} bytes", CodeGenerationPreviewText.size()));
		AppendLogLine(std::format("Opcode size: {} bytes", CodeGenerationOpcodeBytes.size()));
		AppendLogLine(std::format("Score weights: cycles={}, bytes={}", CodeGenerationOptions.CycleWeight, CodeGenerationOptions.ByteWeight));
		AppendLogLine(std::format("Search: beam={}, probe limit={}", CodeGenerationOptions.NonLinearBeamWidth, CodeGenerationOptions.MaxNonLinearCandidatesToEvaluatePerPass));
		AppendLogLine(std::format("Screen target: 0x{:04X}..0x{:04X}",
			CodeGenerationOptions.ScreenBaseAddress,
			static_cast<uint16_t>(CodeGenerationOptions.ScreenBaseAddress + CodeGenerator::ZX_SCREEN_SIZE - 1)));
		AppendLogLine(std::format("Data: pixels={}, attributes={}, frame diff={}",
			CodeGenerationOptions.GeneratePixels ? "on" : "off",
			CodeGenerationOptions.GenerateAttributes ? "on" : "off",
			CodeGenerationOptions.ReverseFrameDifference ? "reverse" : "forward"));
		if (CodeGenerationOptions.ProjectSelection)
		{
			AppendLogLine(std::format("Projection: canvas selection to X={}, Y={}",
				CodeGenerationOptions.DestinationX & ~7,
				CodeGenerationOptions.DestinationY));
		}
		AppendLogLine(std::format("Stack options: max pairs={}, top=0x{:04X}, preserve SP={}, DI/EI={}",
			CodeGenerationOptions.MaxStackPairsToEnumerate,
			CodeGenerationOptions.StackTopAddress,
			CodeGenerationOptions.PreserveSP ? "on" : "off",
			CodeGenerationOptions.DisableInterruptsForStack ? "on" : "off"));
		if (CodeGenerationOptions.PreserveSP)
		{
			AppendLogLine(std::format("Stack scratch: 0x{:04X}..0x{:04X}",
				static_cast<uint16_t>(CodeGenerationOptions.StackTopAddress - 2),
				static_cast<uint16_t>(CodeGenerationOptions.StackTopAddress + 1)));
		}
		AppendLogLine(std::format("Candidates: byte={}, word={}, stack={}, repeat={}, horizontal={}, vertical={}, reverse={}, registers={}",
			CodeGenerationOptions.EnableByteCandidates ? "on" : "off",
			CodeGenerationOptions.EnableWordCandidates ? "on" : "off",
			CodeGenerationOptions.EnableStackBlocks ? "on" : "off",
			CodeGenerationOptions.EnableRepeatWords ? "on" : "off",
			CodeGenerationOptions.EnableHorizontalSameByteIncL ? "on" : "off",
			CodeGenerationOptions.EnableVerticalCandidates ? "on" : "off",
			CodeGenerationOptions.EnableReverseDirections ? "on" : "off",
			CodeGenerationOptions.EnableRegisterConstants ? "on" : "off"));
		AppendLogLine(std::format("Dirty bytes: {}", CodeGenerationJobResult.DirtyBytes));
		AppendLogLine(std::format("Operations: {}", CodeGenerationJobResult.OperationCount));
		AppendLogLine(std::format("Estimated cycles: {}", CodeGenerationJobResult.Cycles));
		AppendLogLine(std::format("Code bytes: {}", CodeGenerationJobResult.CodeBytes));
		if (bCodeGenerationGenerateOpcode)
		{
			AppendLogLine("Opcode generation: enabled.");
			AppendLogLine("Preview remains readable assembly text.");
		}
		AppendLogLine("OK");
	}

	bCodeGenerationProgressShouldClose = true;
}

bool FAppSprite::ShowModal_CodeGenerationProgress()
{
	if (!bCodeGenerationProgressModalOpen && !bCodeGenerationGenerationInProgress.load(std::memory_order_relaxed) && !bCodeGenerationProgressShouldClose)
	{
		return false;
	}

	if (bCodeGenerationProgressShouldClose)
	{
		bCodeGenerationProgressModalOpen = false;
		bCodeGenerationProgressShouldClose = false;
		return false;
	}

	ImGuiWindow* CodeGenerationWindow = ImGui::FindWindowByName(Modal_CodeGenerationName);
	ImGuiViewport* ParentViewport = CodeGenerationWindow && CodeGenerationWindow->Viewport
		? CodeGenerationWindow->Viewport
		: ImGui::GetMainViewport();
	ImGuiWindowClass ProgressWindowClass;
	ProgressWindowClass.ParentViewportId = ParentViewport->ID;
	ProgressWindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoAutoMerge |
		ImGuiViewportFlags_NoTaskBarIcon |
		ImGuiViewportFlags_NoFocusOnAppearing;
	ImGui::SetNextWindowClass(&ProgressWindowClass);
	ImGui::SetNextWindowPos(ParentViewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	const ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize;
	const bool bVisible = ImGui::Begin(Modal_CodeGenerationProgressName, nullptr, WindowFlags);
	if (bVisible)
	{
		const int32_t Current = CodeGenerationProgressCurrent.load(std::memory_order_relaxed);
		const int32_t Total = ImMax(CodeGenerationProgressTotal.load(std::memory_order_relaxed), 1);
		const float Fraction = ImClamp(static_cast<float>(Current) / static_cast<float>(Total), 0.0f, 1.0f);

		ImGui::TextUnformatted("Generating code...");
		ImGui::ProgressBar(Fraction, ImVec2(320.0f, 0.0f));
		ImGui::Text("%d / %d", Current, Total);

		if (bCodeGenerationCancelRequested.load(std::memory_order_relaxed))
		{
			ImGui::TextUnformatted("Cancelling...");
		}
		else
		{
			ImGui::TextUnformatted("Press Cancel to stop the current generation.");
		}

		if (!bCodeGenerationCancelRequested.load(std::memory_order_relaxed))
		{
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
			{
				bCodeGenerationCancelRequested.store(true, std::memory_order_relaxed);
			}
		}
		else
		{
			ImGui::BeginDisabled();
			ImGui::Button("Cancel", ImVec2(120.0f, 0.0f));
			ImGui::EndDisabled();
		}

	}
	ImGui::End();
	return true;
}

bool FAppSprite::ExportCodeGenerationPreview()
{
	auto AppendLogLine = [this](const std::string& Line)
	{
		if (!CodeGenerationLogText.empty())
		{
			CodeGenerationLogText.push_back('\n');
		}
		CodeGenerationLogText += Line;
		bCodeGenerationLogScrollToBottom = true;
	};

	if (!bCodeGenerationPreviewValid || (bCodeGenerationGenerateOpcode ? CodeGenerationOpcodeBytes.empty() : CodeGenerationPreviewText.empty()))
	{
		AppendLogLine("Nothing to export.");
		return false;
	}

	std::filesystem::path OutputPath(Utils::Utf8ToUtf16(NewOutputFileNameBuffer));
	if (OutputPath.empty())
	{
		AppendLogLine("Output filename is empty.");
		return false;
	}

	ApplyCodeGenerationOutputModeToPath(OutputPath, bCodeGenerationGenerateOpcode);

	if (OutputPath.is_relative())
	{
		std::shared_ptr<SCanvas> Canvas = GetActiveCanvas();
		std::filesystem::path BasePath = FAppFramework::GetPath(EPathType::Export);
		if (Canvas && !Canvas->GetSourcePathFile().empty())
		{
			BasePath = Canvas->GetSourcePathFile().parent_path();
		}

		OutputPath = BasePath / OutputPath;
	}

	OutputPath = IO::NormalizePath(std::filesystem::absolute(OutputPath));
	std::vector<uint8_t> AsmBytes;
	const std::vector<uint8_t>& ExportBytes = bCodeGenerationGenerateOpcode
		? CodeGenerationOpcodeBytes
		: (AsmBytes = std::vector<uint8_t>(CodeGenerationPreviewText.begin(), CodeGenerationPreviewText.end()));
	std::error_code Error = IO::SaveBinaryData(ExportBytes, OutputPath, false);
	if (Error)
	{
		AppendLogLine(std::format("Export failed: {}", Error.message()));
		return false;
	}

	AppendLogLine(std::format("Exported: {}", Utils::Utf16ToUtf8(OutputPath.wstring())));
	IO::OpenFolder(OutputPath.parent_path());
	return true;
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

void FAppSprite::Imput_ToggleFrameMode()
{
	if (ViewFlags.FrameMode == EFrameMode::None)
	{
		ViewFlags.FrameMode = FrameDifferenceDirection;
	}
	else
	{
		FrameDifferenceDirection = ViewFlags.FrameMode;
		ViewFlags.FrameMode = EFrameMode::None;
	}
	FEvent_Canvas Canvas_Event(FEventTag::CanvasViewFlagsTag);
	{
		Canvas_Event.CanvasName = {};
		Canvas_Event.ViewFlags = ViewFlags;
		Viewer->GetEventSystem().Publish(Canvas_Event);
	}
}

void FAppSprite::Imput_ToggleFrameDirection()
{
	FrameDifferenceDirection = FrameDifferenceDirection == EFrameMode::ReverseDifference
		? EFrameMode::Difference
		: EFrameMode::ReverseDifference;
	if (ViewFlags.FrameMode != EFrameMode::None)
	{
		ViewFlags.FrameMode = FrameDifferenceDirection;
	}
	FEvent_Canvas Canvas_Event(FEventTag::CanvasViewFlagsTag);
	{
		Canvas_Event.CanvasName = {};
		Canvas_Event.ViewFlags = ViewFlags;
		Viewer->GetEventSystem().Publish(Canvas_Event);
	}
}

bool FAppSprite::OpenFile(const std::filesystem::path& FilePath)
{
	const EImageFormat ImageFormat = SupportImageFormat(FilePath);
	switch (ImageFormat)
	{
	case EImageFormat::PNG:			return Import_Image(Viewer, FilePath, EImageFormat::PNG);
	case EImageFormat::Aseprite:	return Import_Image(Viewer, FilePath, EImageFormat::Aseprite);
	}

	LOG_ERROR("[{}]\t Format not supported.", (__FUNCTION__));
	return false;
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

bool FAppSprite::Callback_OpenFile(const std::filesystem::path& FilePath)
{
	return OpenFile(FilePath);
}

bool FAppSprite::HasCanvasWithTimeline() const
{
	for (std::shared_ptr<SCanvas> Canvas : ActiveCanvas)
	{
		if (Canvas->HasTimeline())
		{
			return true;
		}
	}
	return false;
}

std::shared_ptr<SCanvas> FAppSprite::GetActiveCanvas()
{
	for (std::shared_ptr<SCanvas> Canvas : ActiveCanvas)
	{
		if (Canvas->IsOpen() && Canvas->IsFocus())
		{
			return Canvas;
		}
	}
	return nullptr;
}
