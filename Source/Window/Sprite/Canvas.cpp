#include "Canvas.h"
#include "SpriteList.h"
#include <AppSprite.h>
#include <Utils/UI/Draw.h>
#include <Window/Sprite/Events.h>
#include <Utils/IO.h>
#include <Utils/Aseprite/Format.h>

#include "stb/stb_image_write.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Canvas";
	static const char* PopupMenuName = TEXT("##PopupMenuSprite");
	static const char* CreateSpriteName = "##CreateSprite";

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

SCanvas::SCanvas(EFont::Type _FontName, const std::wstring& Name, const std::filesystem::path& _SourcePathFile)
	: Super(FWindowInitializer()
		.SetName(std::format(L"{}##{}", Name.c_str(), ThisWindowName))
		.SetFontName(_FontName)
		.SetDockSlot("##Layout_Canvas")
		.SetIncludeInWindows(true))
	, bDirty(false)
	, bDragging(false)
	, bRefreshCanvas(false)
	, bRectangleMarqueeActive(false)
	, bNeedConvertCanvasToZX(false)
	, bNeedConvertZXToCanvas(false)
	, bOpenPopupMenu(false)
	, bMouseInsideMarquee(false)
	, LastOptionsFlags(FCanvasOptionsFlags::None)
	, LastSetPixelColorIndex(EZXColor::None)
	, LastSetPixelPosition(-1.0f, -1.0f)
	, Width(0)
	, Height(0)
	, ImageFormat(EImageFormat::None)
	, SourcePathFile(_SourcePathFile)
{
	ButtonColor[0] = EZXColor::Black_;
	ButtonColor[1] = EZXColor::Black;

	Subcolor[ESubcolor::Ink] = EZXColor::Black_;
	Subcolor[ESubcolor::Paper] = EZXColor::White_;
	Subcolor[ESubcolor::Bright] = EZXColor::False;
	Subcolor[ESubcolor::Flash] = EZXColor::False;

	OptionsFlags[0] = FCanvasOptionsFlags::Source;
	OptionsFlags[1] = FCanvasOptionsFlags::None;

	ToolMode[0] = EToolMode::None;
	ToolMode[1] = EToolMode::None;

	ConversationSettings = 
	{
		.InkAlways = EZXColor::Black_,
		.TransparentIndex = EZXColor::Transparent,
		.ReplaceTransparent = EZXColor::Black,
	};
}

void SCanvas::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_Color>(
		[this](const FEvent_Color& Event)
		{
			Event.ButtonIndex;				// pressed mouse button
			Event.SelectedColorIndex;		// zx color
			Event.SelectedSubcolorIndex;	// type ink/paper/bright

			if (Event.Tag == FEventTag::ChangeColorTag)
			{
				ButtonColor[Event.ButtonIndex & 0x01] = Event.SelectedColorIndex;
				if (Event.SelectedSubcolorIndex < ESubcolor::MAX)
				{
					Subcolor[Event.SelectedSubcolorIndex] = Event.SelectedColorIndex;
				}
			}
		});

	SubscribeEvent<FEvent_ToolBar>(
		[this](const FEvent_ToolBar& Event)
		{
			if (Event.Tag == FEventTag::ChangeToolModeTag)
			{
				SetToolMode(Event.ChangeToolMode.ToolMode, true, true);
			}
		});

	SubscribeEvent<FEvent_SelectedSprite>(
		[this](const FEvent_SelectedSprite& Event)
		{
			if (Event.Tag == FEventTag::SelectedSpritesChangedTag)
			{
				SelectedSprites = Event.Sprites;
			}
		});
}

void SCanvas::Initialize(const std::vector<std::any>& Args)
{
	AsepriteFormat::FFrame Frame;

	ZXColorView = std::make_shared<UI::FZXColorView>();
	ZXColorView->Scale = ImVec2(2.5f, 2.5f);
	ZXColorView->ImagePosition = ImVec2(0.0f, 0.0f);

	for (const auto& Arg : Args)
	{
		if (Arg.type() == typeid(std::filesystem::path))
		{
			SourcePathFile = std::any_cast<std::filesystem::path>(Arg);
		}
		else if (Arg.type() == typeid(EImageFormat))
		{
			ImageFormat = std::any_cast<EImageFormat>(Arg);
		}
		else if (Arg.type() == typeid(AsepriteFormat::FFrame))
		{
			Frame = std::any_cast<AsepriteFormat::FFrame>(Arg);
		}
		else if (Arg.type() == typeid(ImVec2))
		{
			const ImVec2 CanvasSize = std::any_cast<ImVec2>(Arg);
			ImageFormat = EImageFormat::Create;

			Width = (int32_t)CanvasSize.x;
			Height = (int32_t)CanvasSize.y;
			const int32_t Size = Width * Height;
			ZXColorView->IndexedData.resize(Size, 0);

			UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);

			UI::FConversationSettings Settings
			{
				.InkAlways = EZXColor::Black_,
				.TransparentIndex = EZXColor::Transparent,
				.ReplaceTransparent = EZXColor::White,
			};
			ConversionToZX(Settings);

			ZXColorView->Device = Data.Device;
			ZXColorView->DeviceContext = Data.DeviceContext;
			Draw_ZXColorView_Initialize(ZXColorView, UI::ERenderType::Canvas);

			{
				FEvent_StatusBar Event;
				Event.Tag = FEventTag::CanvasSizeTag;
				Event.CanvasSize = ImVec2((float)Width, (float)Height);
				SendEvent(Event);
			}
		}
	}

	switch (ImageFormat)
	{
	case EImageFormat::Create:
	{
		LOG_DISPLAY("[{}]\t Create canvas.", (__FUNCTION__));
		break;
	}

	case EImageFormat::PNG:
	{
		if (!SourcePathFile.empty())
		{
			TransparentColor = UI::ToU32(COLOR(0, 0, 0, 0));

			std::filesystem::path LoadPath = SourcePathFile.parent_path();
			std::filesystem::path LoadName = SourcePathFile.stem();

			Load(LoadPath, LoadName);
			
			ZXColorView->Device = Data.Device;
			ZXColorView->DeviceContext = Data.DeviceContext;
			Draw_ZXColorView_Initialize(ZXColorView, UI::ERenderType::Canvas);

			{
				FEvent_StatusBar Event;
				Event.Tag = FEventTag::CanvasSizeTag;
				Event.CanvasSize = ImVec2((float)Width, (float)Height);
				SendEvent(Event);
			}
		}
		break;
	}

	case EImageFormat::Aseprite:
	{
		LOG_WARNING("[{}]\t Should not be supported!", (__FUNCTION__));
		break;
	}

	case EImageFormat::Aseprite_Frame:
	{
		Width = Frame.Width;
		Height = Frame.Height;
		TransparentColor = UI::ToU32(COLOR(0, 0, 0, 0));

		UI::QuantizeToZX(Frame.Image.data(), Width, Height, 4, ZXColorView->IndexedData, TransparentColor);
		UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);
		ConversionToZX(ConversationSettings);

		ZXColorView->Device = Data.Device;
		ZXColorView->DeviceContext = Data.DeviceContext;
		Draw_ZXColorView_Initialize(ZXColorView, UI::ERenderType::Canvas);

		{
			FEvent_StatusBar Event;
			Event.Tag = FEventTag::CanvasSizeTag;
			Event.CanvasSize = ImVec2((float)Width, (float)Height);
			SendEvent(Event);
		}

		break;
	}

	default:
	{
		LOG_ERROR("[{}]\t Unknown format image.", (__FUNCTION__));
		break;
	}

	}
}

void SCanvas::SetupHotKeys()
{
	auto Self = std::dynamic_pointer_cast<SCanvas>(shared_from_this());
	Hotkeys =
	{
		{ ImGuiKey_Escape,								ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_None,				Self)	},	// 
		{ ImGuiKey_M,									ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_RectangleMarquee,	Self)	},	// 
		{ ImGuiKey_B,									ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_Pencil,				Self)	},	// 
		{ ImGuiKey_E,									ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_Eraser,				Self)	},	// 
		{ ImGuiKey_G,									ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_PaintBucket,		Self)	},	// 
		{ ImGuiKey_I,									ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_Eyedropper,			Self)	},	//

		{ ImGuiMod_Ctrl | ImGuiKey_A,					ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SelectAll,						Self)	},	// (ctrl + A)
		{ ImGuiMod_Ctrl | ImGuiKey_C,					ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_Copy,							Self)	},	// (ctrl + C)
		{ ImGuiMod_Ctrl | ImGuiKey_V,					ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_Paste,							Self)	},	// (ctrl + V)
		{ ImGuiMod_Ctrl | ImGuiKey_X,					ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_Cut,							Self)	},	// (ctrl + X)
		{ ImGuiKey_Delete,								ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_Delete,							Self)	},	// (delete)
		{ ImGuiMod_Ctrl | ImGuiKey_Z,					ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_Undo,							Self)	},	// (ctrl + Z)
		{ ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z,	ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_Redo,							Self)	},	// (ctrl + shift + Z)

		// global
		{ ImGuiMod_Ctrl | ImGuiKey_S,					ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteFocused,	std::bind(&ThisClass::Imput_Save,							Self)	},	// (ctrl + S)
	};
}

void SCanvas::Tick(float DeltaTime)
{
	ZXColorView->TimeCounter += DeltaTime;

	for (const std::shared_ptr<FSprite>& Sprite : SelectedSprites)
	{
		Sprite->ZXColorView->TimeCounter += DeltaTime;
	}
}

void SCanvas::Render()
{
	SWindow::Render();

	if (!IsOpen())
	{
		Close();
		return;
	}

	const bool bInk = OptionsFlags[0] & FCanvasOptionsFlags::Ink;
	const bool bMask = OptionsFlags[0] & FCanvasOptionsFlags::Mask;
	const bool bPaper = OptionsFlags[0] & FCanvasOptionsFlags::Attribute;
	const bool bSource = OptionsFlags[0] & FCanvasOptionsFlags::Source;

	if (bRefreshCanvas)
	{
		if (OptionsFlags[0] & FCanvasOptionsFlags::Source)
		{
			UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height);
		}
		else
		{
			UI::ZXAttributeColorToImage(
				ZXColorView->Image,
				Width, Height,
				bInk ? ZXColorView->InkData.data() : nullptr,
				bPaper ? ZXColorView->AttributeData.data() : nullptr,
				bMask ? ZXColorView->MaskData.data() : nullptr);
		}
		bRefreshCanvas = false;
	}

	const bool bNoMove = ToolMode[0] == EToolMode::RectangleMarquee;
	//const std::string Title = /*bDirty ? "* " + GetWindowName() : */GetWindowName();
	//const std::string UniqueID = Title + "##" + GetWindowName();
	ImGui::Begin(GetWindowName().c_str(), &bOpen, bNoMove ? ImGuiWindowFlags_NoMove : ImGuiWindowFlags_None);
	{
		if (bNoMove)
		{
			ImGuiIO& IO = ImGui::GetIO();
			ImGuiStyle& Style = ImGui::GetStyle();
			ImVec2 WindowsPos = ImGui::GetWindowPos();
			ImVec2 WindowsSize = ImGui::GetWindowSize();

			const float TitleHeight = ImGui::GetFontSize() + Style.FramePadding.y * 2.0f;

			const ImVec2 TitleMin = WindowsPos;
			const ImVec2 TitleMax = ImVec2(WindowsPos.x + WindowsSize.x, WindowsPos.y + TitleHeight);

			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
				ImGui::IsMouseHoveringRect(TitleMin, TitleMax, false))
			{
				ImGui::SetWindowPos(ImVec2(WindowsPos.x + IO.MouseDelta.x, WindowsPos.y + IO.MouseDelta.y));
			}
		}

		Input_HotKeys();
		Input_Mouse();
		ApplyToolMode();
		Draw_PopupMenu();

		ImGui::Spacing();
		if (ImGui::Button("Options", { 0.0f, 25.0f }))
		{
			ImGui::OpenPopup("##Context");
		}
		if (ImGui::BeginPopup("##Context"))
		{
			ImGui::Checkbox("Grid", &ZXColorView->Options.bGrid);
			ImGui::Checkbox("Attribute Grid", &ZXColorView->Options.bAttributeGrid);
			ImGui::Checkbox("Alpha Checkerboard", &ZXColorView->Options.bAlphaCheckerboardGrid);
			
			ImGui::SameLine();

			ImGui::ColorEdit4("MyColor##3", (float*)&ZXColorView->TransparentColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha);

			ImGui::EndPopup();
		}

		const float WindowWidth = ImGui::GetWindowContentRegionMax().x;
		const float WidthDirty = 25.0f;
		const float WidthSource = 25.0f;
		const float WidthConvert = 25.0f;
		const float WidthInk = 25.0f;
		const float WidthPaper = 25.0f;
		const float WidthMask = 25.0f;
		const float Spacing = ImGui::GetStyle().ItemSpacing.x;
		const float TotalWidth = WidthDirty + WidthSource + WidthConvert + WidthInk + WidthPaper + WidthMask + Spacing * 5.0f;
		const float StartButtons = WindowWidth - TotalWidth;

		const bool bSourceEnabled = ZXColorView->IndexedData.size() > 0;
		const bool bConvertEnabled = bNeedConvertCanvasToZX || bNeedConvertZXToCanvas;
		const bool bInkEnabled = ZXColorView->InkData.size() > 0;
		const bool bPaperEnabled = ZXColorView->AttributeData.size() > 0;
		const bool bMaskEnabled = ZXColorView->MaskData.size() > 0;

		ImGui::SameLine(StartButtons);
		if (UI::Button("*", bDirty, { WidthDirty, WidthDirty }))
		{
			Imput_Save();
		}
		ImGui::SameLine();
		if (UI::Button("S", bSource, { WidthSource, WidthSource }, bSourceEnabled))
		{
			if (!(OptionsFlags[0] & FCanvasOptionsFlags::Source))
			{
				LastOptionsFlags = OptionsFlags[0];
			}

			OptionsFlags[0] ^= FCanvasOptionsFlags::Source;
			if (OptionsFlags[0] & FCanvasOptionsFlags::Source)
			{
				OptionsFlags[0] &= ~(
					(bInkEnabled   ?	FCanvasOptionsFlags::Ink		: FCanvasOptionsFlags::None) |
					(bPaperEnabled ?	FCanvasOptionsFlags::Attribute	: FCanvasOptionsFlags::None) |
					(bMaskEnabled  ?	FCanvasOptionsFlags::Mask		: FCanvasOptionsFlags::None)
					);
			}
			else
			{
				if (LastOptionsFlags == FCanvasOptionsFlags::None)
				{
					OptionsFlags[0] |=
						(bInkEnabled   ?	FCanvasOptionsFlags::Ink		: FCanvasOptionsFlags::None) |
						(bPaperEnabled ?	FCanvasOptionsFlags::Attribute	: FCanvasOptionsFlags::None) |
						(bMaskEnabled  ?	FCanvasOptionsFlags::Mask		: FCanvasOptionsFlags::None) ;
				}
				else
				{
					OptionsFlags[0] = LastOptionsFlags;
				}
			}
			bRefreshCanvas = true;
		}
		ImGui::SameLine();
		const char* Symbol = !bConvertEnabled ? "=" : bNeedConvertCanvasToZX ? ">" : "<";
		if (UI::Button(Symbol, bSource, { WidthConvert, WidthConvert }, bConvertEnabled))
		{
			if (bNeedConvertCanvasToZX)
			{
				ConversionToZX(ConversationSettings);
				bNeedConvertCanvasToZX = false;
			}
			else if (bNeedConvertZXToCanvas)
			{
				ConversionToCanvas(ConversationSettings);
				bNeedConvertZXToCanvas = false;
			}
			bRefreshCanvas = true;
		}
		ImGui::SameLine();
		if (UI::Button("I", bInk, { WidthInk, WidthInk }, bInkEnabled))
		{
			OptionsFlags[0] ^= FCanvasOptionsFlags::Ink;
			if (OptionsFlags[0] & FCanvasOptionsFlags::Ink)
			{
				OptionsFlags[0] &= ~FCanvasOptionsFlags::Source;
			}

			bRefreshCanvas = true;
		}
		ImGui::SameLine(); 
		if (UI::Button("P", bPaper, { WidthPaper, WidthPaper }, bPaperEnabled))
		{
			OptionsFlags[0] ^= FCanvasOptionsFlags::Attribute;
			if (OptionsFlags[0] & FCanvasOptionsFlags::Attribute)
			{
				OptionsFlags[0] &= ~FCanvasOptionsFlags::Source;
			}
			bRefreshCanvas = true;
		}
		ImGui::SameLine(); 
		if (UI::Button("M", bMask, { WidthMask, WidthMask }, bMaskEnabled))
		{
			OptionsFlags[0] ^= FCanvasOptionsFlags::Mask;
			if (OptionsFlags[0] & FCanvasOptionsFlags::Mask)
			{
				OptionsFlags[0] &= ~FCanvasOptionsFlags::Source;
			}
			bRefreshCanvas = true;
		}

		if (OptionsFlags[0] == FCanvasOptionsFlags::None)
		{
			OptionsFlags[0] |= FCanvasOptionsFlags::Source;
			bRefreshCanvas = true;
		}

		if (OptionsFlags[0] != OptionsFlags[1])
		{
			OptionsFlags[1] = OptionsFlags[0];
			FEvent_Canvas Event;
			Event.Tag = FEventTag::CanvasOptionsFlagsTag;
			Event.OptionsFlags = OptionsFlags[0];
			SendEvent(Event);
		}

		ImGui::BeginChild("Child", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBringToFrontOnFocus);
		CanvasID = ImGui::GetCurrentWindow()->ID;
		UI::Draw_ZXColorView(ZXColorView);
		ImGui::EndChild();

		ImGui::End();
	}

	if (!IsOpen())
	{
		DestroyWindow();
	}
}

void SCanvas::Destroy()
{
	UI::Draw_ZXColorView_Shutdown(ZXColorView);
	UnsubscribeAll();
}

void SCanvas::Draw_PopupMenu()
{
	const ImGuiID CreateSpriteID = ImGui::GetCurrentWindow()->GetID(CreateSpriteName);

	auto AddRegionLambda = [=, this](int32_t SelectIndex) -> bool
		{
			IndexSelectedSprites = SelectIndex;

			const std::shared_ptr<FSprite>& Sprite = SelectedSprites[IndexSelectedSprites];
			ImRect SpriteRect(
				float(Sprite->SpritePositionToImageX), float(Sprite->SpritePositionToImageY),
				float(Sprite->SpritePositionToImageX + Sprite->Width), float(Sprite->SpritePositionToImageY + Sprite->Height));
			ImRect RegionRect = ZXColorView->RectangleMarqueeRect;
			if (!SpriteRect.Overlaps(RegionRect))
			{
				return false;
			}
			RegionRect.ClipWith(SpriteRect);
			ImRect LocalRegionRect = ImRect(
				RegionRect.Min.x - (float)Sprite->SpritePositionToImageX, RegionRect.Min.y - (float)Sprite->SpritePositionToImageY,
				RegionRect.Max.x - (float)Sprite->SpritePositionToImageX, RegionRect.Max.y - (float)Sprite->SpritePositionToImageY);

			FSpriteMetaRegion NewRegion
			{
				.Rect = LocalRegionRect,
				.ZXColorView = std::make_shared<UI::FZXColorView>(),
			};

			NewRegion.ZXColorView->bOnlyNearestSampling = true;
			NewRegion.ZXColorView->Device = Data.Device;
			NewRegion.ZXColorView->DeviceContext = Data.DeviceContext;
			UI::Draw_ZXColorView_Initialize(NewRegion.ZXColorView, UI::ERenderType::Sprite);
			{
				const int32_t Size = Sprite->Width * Sprite->Height;
				std::vector<uint32_t> RGBA(Size, 0);

				if (!NewRegion.ZXColorView->Image.IsValid())
				{
					for (uint32_t y = (uint32_t)NewRegion.Rect.Min.y; y < (uint32_t)NewRegion.Rect.Max.y; ++y)
					{
						for (uint32_t x = (uint32_t)NewRegion.Rect.Min.x; x < (uint32_t)NewRegion.Rect.Max.x; ++x)
						{
							const int8_t Color = EZXColor::Black_;
							const ImU32 ColorRGBA = UI::ToU32(UI::ZXSpectrumColorRGBA[Color]);

							const uint32_t Index = y * Sprite->Width + x;
							RGBA[Index] = ColorRGBA;
						}
					}
					NewRegion.ZXColorView->Image = FImageBase::Get().CreateTexture(RGBA.data(), Sprite->Width, Sprite->Height, D3D11_CPU_ACCESS_READ, D3D11_USAGE_DEFAULT);
				}
			}

			SelectedSprites[IndexSelectedSprites]->Regions.push_back(NewRegion);
			return true;
		};

	if (bOpenPopupMenu = ImGui::BeginPopup(PopupMenuName))
	{
		if (ImGui::MenuItem("Add Sprite"))
		{
			ImGui::OpenPopup(CreateSpriteID);
		}
		
		if (SelectedSprites.size() == 1)
		{
			if (ImGui::MenuItem("Add Region"))
			{
				AddRegionLambda(0);
			}
		}
		else if (SelectedSprites.size() > 1)
		{
			if (ImGui::BeginMenu("Add Region"))
			{
				for (int32_t i = 0; i < SelectedSprites.size(); ++i)
				{
					const std::shared_ptr<FSprite>& Sprite = SelectedSprites[i];
					if (ImGui::MenuItem(Sprite->Name.c_str()))
					{
						AddRegionLambda(i);
					}
				}
				ImGui::EndMenu();
			}
		}

		ImGui::EndPopup();	
	}
	
	Draw_PopupMenu_CreateSprite();
}

void SCanvas::Draw_PopupMenu_CreateSprite()
{
	// always center this window when appearing
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	}

	bool bUpdateSize = false;
	if (ImGui::BeginPopupModal(CreateSpriteName, NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (ImGui::IsWindowAppearing())
		{
			CreateSpriteSize = ZXColorView->RectangleMarqueeRect.GetSize();
			sprintf(CreateSpriteNameBuffer, std::format("Sprite {}", ++SpriteCounter).c_str());
			sprintf(CreateSpriteWidthBuffer, "%i\n", int(CreateSpriteSize.x));
			sprintf(CreateSpriteHeightBuffer, "%i\n", int(CreateSpriteSize.y));

			bRoundingToMultipleEight = true;
			bRectangularSprite = false;

			const ImVec2 OriginalSpriteSize = ZXColorView->RectangleMarqueeRect.GetSize();
			Log2SpriteSize = { powf(2.0f, ceilf(log2f(OriginalSpriteSize.x))), powf(2.0f, ceilf(log2f(OriginalSpriteSize.y))) };

			bUpdateSize = true;
		}

		const float TextWidth = ImGui::CalcTextSize("A").x;
		const float TextHeight = ImGui::GetTextLineHeightWithSpacing();

		const ImGuiInputTextFlags InputNumberTextFlags = ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackEdit;

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
		ImGui::Text("Name : ");
		ImGui::SameLine(50.0f);
		ImGui::InputTextEx("##Name", NULL, CreateSpriteNameBuffer, IM_ARRAYSIZE(CreateSpriteNameBuffer), ImVec2(TextWidth * 20.0f, TextHeight), ImGuiInputTextFlags_None);
		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
		ImGui::SeparatorText("Size : ");

		if (ImGui::Checkbox("Multiple of 8", &bRoundingToMultipleEight))
		{
			if (!bRoundingToMultipleEight)
			{
				const ImVec2 OriginalSpriteSize = ZXColorView->RectangleMarqueeRect.GetSize();
				Log2SpriteSize = { powf(2.0f, ceilf(log2f(OriginalSpriteSize.x))), powf(2.0f, ceilf(log2f(OriginalSpriteSize.y))) };
				CreateSpriteSize = ZXColorView->RectangleMarqueeRect.GetSize();
			}
			bUpdateSize = true;
		}
		if (ImGui::Checkbox("Rectangular sprite", &bRectangularSprite))
		{
			if (!bRectangularSprite)
			{
				const ImVec2 OriginalSpriteSize = ZXColorView->RectangleMarqueeRect.GetSize();
				Log2SpriteSize = { powf(2.0f, ceilf(log2f(OriginalSpriteSize.x))), powf(2.0f, ceilf(log2f(OriginalSpriteSize.y))) };
				CreateSpriteSize = ZXColorView->RectangleMarqueeRect.GetSize();
			}

			bUpdateSize = true;
		}
		ImGui::Dummy(ImVec2(0.0f, TextHeight * 1.0f));

		ImGui::Text("Width :");
		ImGui::SameLine(50.0f);
		ImGui::InputTextEx("##Width", NULL, CreateSpriteWidthBuffer, IM_ARRAYSIZE(CreateSpriteWidthBuffer), ImVec2(TextWidth * 10.0f, TextHeight), InputNumberTextFlags, &TextEditNumberCallback, (void*)&CreateSpriteSize.x);
		ImGui::SameLine(150.0f);

		if (ImGui::SliderFloat("##FineTuningX", &CreateSpriteSize.x, 8.0f, Log2SpriteSize.x, "%.0f"))
		{
			if (bRectangularSprite)
			{
				CreateSpriteSize.y = CreateSpriteSize.x;
				Log2SpriteSize.y = Log2SpriteSize.x;
			}
			bUpdateSize = true;
		}

		ImGui::Text("Height :");
		ImGui::SameLine(50.0f);
		ImGui::InputTextEx("##Height", NULL, CreateSpriteHeightBuffer, IM_ARRAYSIZE(CreateSpriteHeightBuffer), ImVec2(TextWidth * 10.0f, TextHeight), InputNumberTextFlags, &TextEditNumberCallback, (void*)&CreateSpriteSize.y);
		ImGui::SameLine(150.0f);
		if (ImGui::SliderFloat("##FineTuningY", &CreateSpriteSize.y, bRoundingToMultipleEight ? 8.0f : 1.0f, Log2SpriteSize.y, "%.0f"))
		{
			if (bRectangularSprite)
			{
				CreateSpriteSize.x = CreateSpriteSize.y;
				Log2SpriteSize.x = Log2SpriteSize.y;
			}

			bUpdateSize = true;
		}

		if (bUpdateSize)
		{
			if (bRectangularSprite)
			{
				const float MinSize = ImMin(CreateSpriteSize.x, CreateSpriteSize.y);
				CreateSpriteSize = { MinSize, MinSize };
			}
			if (bRoundingToMultipleEight)
			{
				CreateSpriteSize.x = ceilf(CreateSpriteSize.x / 8.0f) * 8.0f;
				CreateSpriteSize.y = ceilf(CreateSpriteSize.y / 8.0f) * 8.0f;
			}
			sprintf(CreateSpriteWidthBuffer, "%i\n", int32_t(CreateSpriteSize.x));
			sprintf(CreateSpriteHeightBuffer, "%i\n", int32_t(CreateSpriteSize.y));
		}

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 1.0f));

		if (ImGui::ButtonEx("OK", ImVec2(TextWidth * 11.0f, TextHeight * 1.5f)))
		{
			ImRect SpriteRect(ZXColorView->RectangleMarqueeRect.Min, ZXColorView->RectangleMarqueeRect.Min + CreateSpriteSize);
			{	
				// clamp right-down
				SpriteRect.Min = ImClamp(SpriteRect.Min, ImVec2(0, 0), ZXColorView->Image.Size);
				SpriteRect.Max = ImClamp(SpriteRect.Max, ImVec2(0, 0), ZXColorView->Image.Size);

				// clamp left-up
				SpriteRect.Min = ImClamp(SpriteRect.Max - CreateSpriteSize, ImVec2(0, 0), ZXColorView->Image.Size);
			}

			FEvent_Sprite Event;
			Event.Tag = FEventTag::AddSpriteTag;
			Event.Width = Width;
			Event.Height = Height;
			Event.SpriteRect = SpriteRect;
			Event.SpriteName = CreateSpriteNameBuffer;
			Event.SourcePathFile = SourcePathFile;

			Event.IndexedData = ZXColorView->IndexedData;
			Event.InkData = ZXColorView->InkData;
			Event.AttributeData = ZXColorView->AttributeData;
			Event.MaskData = ZXColorView->MaskData;

			SendEvent(Event);

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
}

void SCanvas::Input_HotKeys()
{
	Shortcut::Handler(Hotkeys);

	ImGuiContext& Context = *ImGui::GetCurrentContext();
	ImGuiWindow* Window = Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != CanvasID)
	{
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiMod_Alt) && !IsEqualToolMode(EToolMode::Eyedropper))
	{
		SetToolMode(EToolMode::Eyedropper, false);
	}
	else if (ImGui::IsKeyReleased(ImGuiMod_Alt)/* && !IsEqualToolMode(EToolMode::None, 1)*/)
	{
		SetToolMode(ToolMode[1], false);
	}
}

void SCanvas::Input_Mouse()
{
	bDragging = false;

	ImGuiContext& Context = *ImGui::GetCurrentContext();
	const float MouseWheel = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner) ? Context.IO.MouseWheel : 0.0f;

	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != CanvasID)
	{
		return;
	}

	{
		ZXColorView->CursorPosition = UI::GetMouse(ZXColorView);
		FEvent_StatusBar Event;
		Event.Tag = FEventTag::MousePositionTag;
		Event.MousePosition = ZXColorView->CursorPosition;
		SendEvent(Event);
	}

	if (MouseWheel != 0.0f && !Context.IO.FontAllowUserScaling)
	{
		UI::Set_ZXViewScale(ZXColorView, MouseWheel);
	}
	else if (!bDragging && Context.IO.MouseDown[ImGuiMouseButton_Middle])
	{
		bDragging = true;
	}
	else if (Context.IO.MouseReleased[ImGuiMouseButton_Middle])
	{
		bDragging = false;
	}
	// dragging
	if (bDragging)
	{
		// hard reset any popup menu
		ImGui::CloseCurrentPopup();

		const ImVec2 Delta = ImGui::GetIO().MouseDelta;
		if (Delta.x != 0 || Delta.y != 0)
		{
			UI::Add_ZXViewDeltaPosition(ZXColorView, Delta);
		}
	}
}

void SCanvas::SetToolMode(EToolMode::Type NewToolMode, bool bForce /*= true*/, bool bEvent /*= false*/)
{
	if (ToolMode[0] != NewToolMode)
	{
		ToolMode[1] = bForce ? EToolMode::None : ToolMode[0];
		ToolMode[0] = NewToolMode;
	}
	else if (ToolMode[1] != EToolMode::None)
	{
		if (ToolMode[1] != NewToolMode)
		{
			ToolMode[0] = ToolMode[1];
		}
		else
		{
			ToolMode[0] = EToolMode::None;
		}
		ToolMode[1] = EToolMode::None;
	}

	if (!bEvent)
	{
		FEvent_Canvas Event;
		Event.Tag = FEventTag::ChangeToolModeTag;
		Event.ChangeToolMode.ToolMode = NewToolMode;
		SendEvent(Event);
	}
}

void SCanvas::ApplyToolMode()
{
	ImGuiContext& Context = *ImGui::GetCurrentContext();
	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != CanvasID)
	{
		return;
	}

	bMouseInsideMarquee = ZXColorView->RectangleMarqueeRect.Contains(UI::ConverZXViewPositionToPixel(*ZXColorView, ImGui::GetMousePos()));

	switch (ToolMode[0])
	{
	case EToolMode::None:
		ZXColorView->bCursorEnable = false;
		Reset_RectangleMarquee();
		break;
	case EToolMode::RectangleMarquee:
		ZXColorView->bCursorEnable = false;
		Handler_RectangleMarquee();
		break;
	case EToolMode::Pencil:
		ZXColorView->bCursorEnable = true;
		Handler_Pencil();
		break;
	case EToolMode::Eraser:
		break;
	case EToolMode::Eyedropper:
		ZXColorView->bCursorEnable = false;
		Handler_Eyedropper();
		break;
	case EToolMode::PaintBucket:
		break;
	}
}

void SCanvas::Imput_Paste()
{
	FRGBAImage ClipboardImage;
	if (Window::ClipboardData(ClipboardImage))
	{
		if (ClipboardImage.Width == Width && ClipboardImage.Height == Height)
		{
			UI::QuantizeToZX(ClipboardImage.Data.data(), Width, Height, 4, ZXColorView->IndexedData, UI::ToU32(COLOR(0, 0, 0, 255)));
			UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height);
			ConversionToZX(ConversationSettings);
			{
				FEvent_StatusBar Event;
				Event.Tag = FEventTag::CanvasSizeTag;
				Event.CanvasSize = ImVec2((float)Width, (float)Height);
				SendEvent(Event);
			}
		}
		else
		{
			// ToDo: ...
		}
	}
}

void SCanvas::Imput_Delete()
{
	if (!bRectangleMarqueeActive)
	{
		ImRect FullRect;

		if (ZXColorView->RectangleMarqueeRect.GetSize() != ImVec2())
		{
			FullRect = ZXColorView->RectangleMarqueeRect;
		}

		if (OptionsFlags[0] & FCanvasOptionsFlags::Source)
		{
			UI::FillRegion(FullRect, ZXColorView->IndexedData, Width, Height, EZXColor::Transparent);
			bNeedConvertCanvasToZX = true;
		}
		else
		{
			const uint8_t Flags = OptionsFlags[0] & ~FCanvasOptionsFlags::Source;

			UI::FillRegion(FullRect,
				(int32_t)ZXColorView->Image.Width, (int32_t)ZXColorView->Image.Height,
				Flags & FCanvasOptionsFlags::Ink ? ZXColorView->InkData.data() : nullptr, 
				Flags & FCanvasOptionsFlags::Attribute ? ZXColorView->AttributeData.data() : nullptr,
				Flags & FCanvasOptionsFlags::Mask ? ZXColorView->MaskData.data() : nullptr,
				EZXColor::False,
				EZXColor::False,
				EZXColor::Black,
				EZXColor::White,
				EZXColor::False,
				EZXColor::True);
			bNeedConvertZXToCanvas = true;
		}
	}

	bRefreshCanvas = true;
}

void SCanvas::Imput_Undo()
{
	UndoQueue.Undo();
}

void SCanvas::Imput_Redo()
{
	UndoQueue.Redo();
}

void SCanvas::Imput_Save()
{
	if (SourcePathFile.empty() || !bDirty)
	{
		return;
	}
	std::filesystem::path SavePath = SourcePathFile.parent_path();
	std::filesystem::path SaveName = SourcePathFile.stem();
	bDirty = !Save(SavePath, SaveName);
}

void SCanvas::Reset_RectangleMarquee()
{
	ZXColorView->RectangleMarqueeRect = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
	ZXColorView->bVisibilityRectangleMarquee = false;
	bRectangleMarqueeActive = false;
	bOpenPopupMenu = false;
}

void SCanvas::Handler_RectangleMarquee()
{
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bHovered = ImGui::IsWindowHovered();

	const bool Shift = IO.KeyShift;
	const bool Ctrl = IO.ConfigMacOSXBehaviors ? IO.KeySuper : IO.KeyCtrl;
	const bool Alt = IO.ConfigMacOSXBehaviors ? IO.KeyCtrl : IO.KeyAlt;

	if (IO.MouseReleased[ImGuiMouseButton_Right])
	{
		if (bMouseInsideMarquee)
		{
			ImGui::OpenPopup(PopupMenuName);
		}
	}

	if (!bOpenPopupMenu && !bRectangleMarqueeActive &&ImGui::IsKeyDown(ImGuiKey_MouseLeft))
	{
		ZXColorView->RectStart = UI::ConverZXViewPositionToPixel(*ZXColorView, ImGui::GetMousePos());
		ZXColorView->RectEnd = ZXColorView->RectStart;

		ZXColorView->bVisibilityRectangleMarquee = true;
		bRectangleMarqueeActive = true;
	}
	else if (!bOpenPopupMenu && bRectangleMarqueeActive && ImGui::IsKeyDown(ImGuiKey_MouseLeft))
	{
		ZXColorView->RectEnd = UI::ConverZXViewPositionToPixel(*ZXColorView, ImGui::GetMousePos());

		ImVec2 p1 = ZXColorView->RectStart;
		ImVec2 p2 = ZXColorView->RectEnd;

		// normalize the rectangle (Min is always to the left/above, Max is to the right/below)
		ZXColorView->RectangleMarqueeRect.Min = ImVec2(ImMin(p1.x, p2.x), ImMin(p1.y, p2.y));
		ZXColorView->RectangleMarqueeRect.Max = ImVec2(ImMax(p1.x, p2.x), ImMax(p1.y, p2.y));

		// last pixel inclusion compensation
		ZXColorView->RectangleMarqueeRect.Max.x += 1.0f;
		ZXColorView->RectangleMarqueeRect.Max.y += 1.0f;

		ZXColorView->RectangleMarqueeRect.Min = ImClamp(ZXColorView->RectangleMarqueeRect.Min, ImVec2(0, 0), ZXColorView->Image.Size);
		ZXColorView->RectangleMarqueeRect.Max = ImClamp(ZXColorView->RectangleMarqueeRect.Max, ImVec2(0, 0), ZXColorView->Image.Size);
	}
	else if (ImGui::IsKeyReleased(ImGuiKey_MouseLeft))
	{
		bRectangleMarqueeActive = false;
	}
	else if (bOpenPopupMenu &&ZXColorView->bVisibilityRectangleMarquee && ImGui::IsKeyPressed(ImGuiKey_MouseLeft))
	{
		bOpenPopupMenu = false;
	}

	//ImGui::Text("Min (%f, %f), Max (%f, %f)",
	//	ZXColorView->RectangleMarqueeRect.Min.x, ZXColorView->RectangleMarqueeRect.Min.y,
	//	ZXColorView->RectangleMarqueeRect.Max.x, ZXColorView->RectangleMarqueeRect.Max.y);
}

void SCanvas::Handler_Pencil()
{
	ImGuiContext& Context = *ImGui::GetCurrentContext();
	if (!Context.IO.MouseDown[ImGuiMouseButton_Left] && 
		!Context.IO.MouseDown[ImGuiMouseButton_Right])
	{
		if (UndoQueue.IsContinuous())
		{
			UndoQueue.EndContinuous();
			size_t PixelsStroke = UndoQueue.UndoSize() - PixelStrokeBegin;
			if (PixelsStroke == 0)
			{
				return;
			}
			UndoQueue.Stroke(PixelsStroke);
		}
		return;
	}

	if (ZXColorView->bVisibilityRectangleMarquee && (!ZXColorView->bVisibilityRectangleMarquee || !bMouseInsideMarquee))
	{
		return;
	}

	if (!UndoQueue.IsContinuous())
	{
		UndoQueue.BeginContinuous();
		PixelStrokeBegin = UndoQueue.UndoSize();
	}

	const int8_t ButtonIndex = Context.IO.MouseDown[ImGuiMouseButton_Left] ? 0 : 1;
	const float X = FMath::Clamp((float)FMath::FloorToInt32(ZXColorView->CursorPosition.x), 0.0f, (float)Width - 1);
	const float Y = FMath::Clamp((float)FMath::FloorToInt32(ZXColorView->CursorPosition.y), 0.0f, (float)Height - 1);
	Set_PixelToCanvas({ X, Y }, ButtonIndex);
}

void SCanvas::Handler_Eyedropper()
{
	ImGuiContext& Context = *ImGui::GetCurrentContext();
	if (!Context.IO.MouseDown[ImGuiMouseButton_Left] && !Context.IO.MouseDown[ImGuiMouseButton_Right])
	{
		return;
	}
	const uint8_t ButtonIndex = Context.IO.MouseDown[ImGuiMouseButton_Left] ? 0 : 1;
	const float X = FMath::Clamp((float)FMath::FloorToInt32(ZXColorView->CursorPosition.x), 0.0f, (float)Width - 1);
	const float Y = FMath::Clamp((float)FMath::FloorToInt32(ZXColorView->CursorPosition.y), 0.0f, (float)Height - 1);

	if (OptionsFlags[0] & FCanvasOptionsFlags::Source)
	{
		const uint32_t Offset = (uint32_t)Y * Width + (uint32_t)X;
		ButtonColor[ButtonIndex] = (UI::EZXSpectrumColor::Type)ZXColorView->IndexedData[Offset];

		FEvent_Color Event;
		{
			Event.Tag = FEventTag::ChangeColorTag;
			Event.ButtonIndex = ButtonIndex;								// pressed mouse button
			Event.SelectedColorIndex = ButtonColor[ButtonIndex];			// zx color
			Event.SelectedSubcolorIndex = (ESubcolor::Type)ButtonIndex;		// ink (LKM), paper (RKM)
		}
		SendEvent(Event);
		
		UpdateCursorColor(true);
	}
	else
	{
		const int32_t Boundary_X = Width >> 3;
		const int32_t Boundary_Y = Height >> 3;

		const int32_t x = (uint32_t)X;
		const int32_t y = (uint32_t)Y;

		const int32_t bx = x / 8;
		const int32_t dx = x % 8;
		const int32_t by = y / 8;
		const int32_t dy = y % 8;

		const int32_t InkMaskOffset = (by * 8 + dy) * Boundary_X + bx;
		uint8_t& Pixels = ZXColorView->InkData[InkMaskOffset];
		uint8_t& Mask = ZXColorView->MaskData[InkMaskOffset];

		const int32_t AttributeOffset = by * Boundary_X + bx;
		uint8_t& Attribute = ZXColorView->AttributeData[AttributeOffset];

		const bool bAttributeBright = (Attribute >> 6) & 0x01;
		const uint8_t AttributeInkColor = (Attribute & 0x07);
		const uint8_t AttributePaperColor = ((Attribute >> 3) & 0x07);

		const uint8_t PixelBit = 1 << (7 - dx);

		const uint8_t Flags = OptionsFlags[0] & ~FCanvasOptionsFlags::Source;
		switch (Flags)
		{
		case FCanvasOptionsFlags::Ink:																// 0010
			Subcolor[ESubcolor::Ink] = (EZXColor)AttributeInkColor;
			break;
		case FCanvasOptionsFlags::Attribute:														// 0100
		case FCanvasOptionsFlags::Ink | FCanvasOptionsFlags::Attribute:								// 0110
			Subcolor[ESubcolor::Ink] = EZXColor(AttributeInkColor == EZXColor::Transparent ? EZXColor::Black_ : AttributeInkColor);
			Subcolor[ESubcolor::Paper] = EZXColor(AttributePaperColor == EZXColor::Transparent ? EZXColor::Black_ : AttributePaperColor);
			Subcolor[ESubcolor::Bright] = bAttributeBright ? EZXColor::True : EZXColor::False;
			break;
		case FCanvasOptionsFlags::Mask:																// 1000
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Ink:									// 1010
			Subcolor[ESubcolor::Ink] = EZXColor(AttributeInkColor == EZXColor::Transparent ? EZXColor::Black_ : AttributeInkColor);
			break;
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Attribute:							// 1100
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Attribute | FCanvasOptionsFlags::Ink:	// 1110
			Subcolor[ESubcolor::Ink] = EZXColor(AttributeInkColor == EZXColor::Transparent ? EZXColor::Black_ : AttributeInkColor);
			Subcolor[ESubcolor::Paper] = EZXColor(AttributePaperColor == EZXColor::Transparent ? EZXColor::Black_ : AttributePaperColor);
			Subcolor[ESubcolor::Bright] = bAttributeBright ? EZXColor::True : EZXColor::False;
			break;
		}
		FEvent_Color Event;
		{
			Event.Tag = FEventTag::ChangeColorTag;
			Event.ButtonIndex = INDEX_NONE;										// pressed mouse button
			Event.SelectedColorIndex = UI::EZXSpectrumColor::Type(Attribute);	// zx color
			Event.SelectedSubcolorIndex = ESubcolor::All;						// ink (LKM), paper (RKM)
		}
		SendEvent(Event); 
		
		UpdateCursorColor();
	}
}

bool SCanvas::Save(const std::filesystem::path& SavePath, const std::filesystem::path& SaveName)
{
	FImageBase& Images = FImageBase::Get();
	std::vector<uint32_t> RGBA;
	UI::ZXIndexColorToRGBA(RGBA, ZXColorView->IndexedData, Width, Height);

	constexpr int32_t Channels = 4;
	if (!stbi_write_png(
		SourcePathFile.string().c_str(),
		Width,
		Height,
		Channels,
		RGBA.data(),
		Width * Channels))
	{
		std::cerr << "Failed to write PNG!" << std::endl;
		LOG_ERROR("[{}]\t Failed to write PNG!", (__FUNCTION__));
		return false;
	}

	std::filesystem::path InkDataFilePath;
	if (ZXColorView->InkData.size() > 0)
	{
		InkDataFilePath = IO::NormalizePath(std::filesystem::absolute(SavePath / std::format("{}.ink", SaveName.string())));
		IO::SaveBinaryData(ZXColorView->InkData, InkDataFilePath, false);
	}

	std::filesystem::path AttributeDataFilePath;
	if (ZXColorView->AttributeData.size() > 0)
	{
		AttributeDataFilePath = IO::NormalizePath(std::filesystem::absolute(SavePath / std::format("{}.attr", SaveName.string())));
		IO::SaveBinaryData(ZXColorView->AttributeData, AttributeDataFilePath, false);
	}

	std::filesystem::path MaskDataFilePath;
	if (ZXColorView->MaskData.size() > 0)
	{
		MaskDataFilePath = IO::NormalizePath(std::filesystem::absolute(SavePath / std::format("{}.mask", SaveName.string())));
		IO::SaveBinaryData(ZXColorView->MaskData, MaskDataFilePath, false);
	}

	return true;
}

bool SCanvas::Load(const std::filesystem::path& LoadPath, const std::filesystem::path& LoadName)
{
	uint8_t* ImageData = FImageBase::LoadToMemory(SourcePathFile, Width, Height);
	UI::QuantizeToZX(ImageData, Width, Height, 4, ZXColorView->IndexedData, TransparentColor);
	UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);
	ConversionToZX(ConversationSettings);
	FImageBase::ReleaseLoadedIntoMemory(ImageData);

	// ink
	{
		std::filesystem::path InkDataFilePath = IO::NormalizePath(std::filesystem::absolute(LoadPath / std::format("{}.ink", LoadName.string())));
		IO::LoadBinaryData(ZXColorView->InkData, InkDataFilePath);
	}

	// attribute
	{
		std::filesystem::path AttributeDataFilePath = IO::NormalizePath(std::filesystem::absolute(LoadPath / std::format("{}.attr", LoadName.string())));
		IO::LoadBinaryData(ZXColorView->AttributeData, AttributeDataFilePath);
	}

	// mask
	{
		std::filesystem::path MaskDataFilePath = IO::NormalizePath(std::filesystem::absolute(LoadPath / std::format("{}.mask", LoadName.string())));
		IO::LoadBinaryData(ZXColorView->MaskData, MaskDataFilePath);
	}

	return true;
}

void SCanvas::ConversionToZX(const UI::FConversationSettings& Settings)
{
	UI::ZXIndexColorToZXAttributeColor(
		ZXColorView->IndexedData,
		Width, Height,
		ZXColorView->InkData,
		ZXColorView->AttributeData,
		ZXColorView->MaskData,
		Settings);
	UI::ZXIndexColorToImage(
		ZXColorView->Image,
		ZXColorView->IndexedData,
		Width, Height);
}

void SCanvas::ConversionToCanvas(const UI::FConversationSettings& Settings)
{
	UI::ZXAttributeColorToZXIndexColor(
		ZXColorView->Image,
		Width, Height,
		ZXColorView->IndexedData,
		ZXColorView->InkData,
		ZXColorView->AttributeData,
		ZXColorView->MaskData);
}

void SCanvas::Set_PixelToCanvas(const ImVec2& Position, uint8_t ButtonIndex)
{
	const uint8_t ColorIndex = ButtonColor[ButtonIndex];
	if (LastSetPixelPosition == Position &&
		LastSetButtonIndex == ButtonIndex &&
		LastSetPixelColorIndex == ColorIndex)
	{
		return;
	}

	LastSetPixelPosition = Position;
	LastSetButtonIndex = ButtonIndex;
	LastSetPixelColorIndex = ColorIndex;

	FPixelToCanvas Pixel;
	{
		Pixel.Position.push_back(Position);
		Pixel.Color.push_back(ColorIndex);
		Pixel.Canvas = OptionsFlags[0];
	}
	UndoQueue.SetWithUndo(
		std::make_shared<Undo::TAction<ThisClass, FPixelToCanvas>>(
			std::bind(&ThisClass::UndoSwapPixel, this, std::placeholders::_1),
			std::bind(&ThisClass::UndoUnitePixels, this, std::placeholders::_1, std::placeholders::_2),
			Pixel
		)
	);

	bDirty = true;
}

void SCanvas::UpdateCursorColor(bool bButton /*= false*/)
{
	if (bButton)
	{
		ZXColorView->CursorColor = UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[0]]);
	}
	else
	{
		const bool bBright = Subcolor[ESubcolor::Bright] == EZXColor::True;
		const bool bFlash = Subcolor[ESubcolor::Flash] == EZXColor::True;
		const uint8_t InkColor = Subcolor[ESubcolor::Ink] == EZXColor::Transparent ? EZXColor::Transparent : Subcolor[ESubcolor::Ink] | (bBright << 3);
		const uint8_t PaperColor = Subcolor[ESubcolor::Paper] == EZXColor::Transparent ? EZXColor::Transparent : Subcolor[ESubcolor::Paper] | (bBright << 3);

		ZXColorView->CursorColor = UI::ToVec4(UI::ZXSpectrumColorRGBA[InkColor]);
	}
}

void SCanvas::UndoSwapPixel(FPixelToCanvas& Param)
{
	std::swap(OptionsFlags[0], Param.Canvas);

	if (OptionsFlags[0] & FCanvasOptionsFlags::Source)
	{
		for (int32_t Index = (int32_t)Param.Color.size() - 1; Index >= 0; --Index)
		{
			const ImVec2& Position = Param.Position[Index];
			uint32_t& Color = Param.Color[Index];
			const uint32_t Offset = (uint32_t)Position.y * Width + (uint32_t)Position.x;
			std::swap(ZXColorView->IndexedData[Offset], (uint8_t&)Color);
		}
		bNeedConvertCanvasToZX = true;
	}
	else
	{
		for (int32_t Index = (int32_t)Param.Color.size() - 1; Index >= 0 ; --Index)
		{
			const ImVec2& Position = Param.Position[Index];
			uint32_t& Color = Param.Color[Index];

			const int32_t Boundary_X = Width >> 3;
			const int32_t Boundary_Y = Height >> 3;

			const int32_t x = (uint32_t)Position.x;
			const int32_t y = (uint32_t)Position.y;

			const int32_t bx = x / 8;
			const int32_t dx = x % 8;
			const int32_t by = y / 8;
			const int32_t dy = y % 8;

			const int32_t InkMaskOffset = (by * 8 + dy) * Boundary_X + bx;
			uint8_t& Pixels = ZXColorView->InkData[InkMaskOffset];
			uint8_t& Mask = ZXColorView->MaskData[InkMaskOffset];
			uint8_t& Attribute = ZXColorView->AttributeData[by * Boundary_X + bx];

			// swap pixel color
			const uint8_t PixelBit = 1 << (7 - dx);
			const uint8_t Flags = OptionsFlags[0] & ~FCanvasOptionsFlags::Source;

			// swap pixel bit
			if (Flags & FCanvasOptionsFlags::Ink && Subcolor[ESubcolor::Ink] != EZXColor::Transparent)
			{
				uint8_t& PixelsByte = reinterpret_cast<uint8_t*>(&Color)[3];
				uint8_t Diff = (Pixels ^ PixelsByte) & PixelBit;
				Pixels ^= Diff;
				PixelsByte ^= Diff;
			}
			// swap mask bit
			if (Flags & FCanvasOptionsFlags::Mask && Subcolor[ESubcolor::Ink] != EZXColor::Transparent)
			{
				uint8_t& MaskByte = reinterpret_cast<uint8_t*>(&Color)[2];
				uint8_t Diff = (Mask ^ MaskByte) & PixelBit;
				Mask ^= Diff;
				MaskByte ^= Diff;
			}
			// swap byte attribute
			if (Flags & FCanvasOptionsFlags::Attribute && Subcolor[ESubcolor::Paper] != EZXColor::Transparent)
			{
				std::swap(Attribute, reinterpret_cast<uint8_t*>(&Color)[1]);
			}

			// set pixel color
			uint8_t& _Color = reinterpret_cast<uint8_t*>(&Color)[0];
			if (_Color != EZXColor::None)
			{
				if (Flags & FCanvasOptionsFlags::Ink)
				{
					if (_Color != EZXColor::Transparent)
					{
						const uint8_t PixelBit = 1 << (7 - dx);
						const bool bOperation = (_Color & 0x07) != EZXColor::White;
						if (bOperation)
						{
							Pixels |= PixelBit;									// set bit
						}
						else
						{
							Pixels &= ~(PixelBit);								// reset bit
						}
					}
				}
				if (Flags & FCanvasOptionsFlags::Mask)
				{
					const uint8_t PixelBit = 1 << (7 - dx);
					const bool bOperation = _Color != EZXColor::Transparent;
					if (bOperation)
					{
						Mask |= PixelBit;									// set bit
					}
					else
					{
						Mask &= ~(PixelBit);								// reset bit
					}
				}
				if (Flags & FCanvasOptionsFlags::Attribute)
				{
					const bool bInkTransparent = Subcolor[ESubcolor::Ink] == EZXColor::Transparent;
					const bool bPaperTransparent = Subcolor[ESubcolor::Paper] == EZXColor::Transparent;

					const uint8_t InkColor = bInkTransparent ? (Attribute & 0x07) : Subcolor[ESubcolor::Ink] & 0x07;
					const uint8_t PaperColor = bPaperTransparent ? ((Attribute >> 3) & 0x07) : Subcolor[ESubcolor::Paper] & 0x07;

					const bool bBright = Subcolor[ESubcolor::Bright] == EZXColor::True;
					const bool bFlash = Subcolor[ESubcolor::Flash] == EZXColor::True;

					Attribute = (bFlash << 7) | (bBright << 6) | (PaperColor << 3) | InkColor;
				}

				_Color = EZXColor::None;
			}
		}
		bNeedConvertZXToCanvas = true;
	}
	bRefreshCanvas = true;
}

void SCanvas::UndoUnitePixels(FPixelToCanvas& A, const FPixelToCanvas& B)
{
	for (int IndexB = 0; IndexB < B.Position.size(); ++IndexB)
	{
		bool bFound = false;
		const ImVec2& PosB = B.Position[IndexB];

		//const uint32_t& ColorB = B.Color[IndexB];
		//const uint8_t& _Color = reinterpret_cast<const uint8_t*>(&ColorB)[0];
		//uint8_t AttributeB = reinterpret_cast<const uint8_t*>(&ColorB)[1];
		//if (_Color == UI::EZXSpectrumColor::None)
		//{
		//	for (int IndexA = 0; IndexA < A.Position.size(); ++IndexA)
		//	{
		//		const ImVec2& PosA = A.Position[IndexA];
		//		if (PosA == PosB)
		//		{
		//			bFound = true;
		//			break;
		//		}
		//
		//		const int32_t PosA_x = (int32_t)PosA.x >> 3;
		//		const int32_t PosA_y = (int32_t)PosA.y >> 3;
		//		const int32_t PosB_x = (int32_t)PosB.x >> 3;
		//		const int32_t PosB_y = (int32_t)PosB.y >> 3;
		//
		//		if (PosA_x != PosB_x || PosA_y != PosB_y)
		//		{
		//			continue;
		//		}
		//
		//		uint32_t& ColorA = A.Color[IndexA]; 
		//		uint8_t& AttributeA = reinterpret_cast<uint8_t*>(&ColorA)[1];
		//		if (AttributeB != AttributeA)
		//		{
		//			bFound = true;
		//		}
		//	}
		//}
		//else
		{
			for (int IndexA = 0; IndexA < A.Position.size(); ++IndexA)
			{
				const ImVec2& PosA = A.Position[IndexA];
				if (PosA == PosB)
				{
					bFound = true;
					break;
				}
			}
		}

		if (!bFound)
		{
			A.Position.push_back(B.Position[IndexB]);
			A.Color.push_back(B.Color[IndexB]);
		}
	}
}
