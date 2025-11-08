#include "Canvas.h"
#include "Events.h"
#include "SpriteList.h"
#include <Utils/UI/Draw.h>

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
	, bDragging(false)
	, bRefreshCanvas(false)
	, bRectangleMarqueeActive(false)
	, bNeedConvertCanvasToZX(false)
	, bNeedConvertZXToCanvas(false)
	, bOpenPopupMenu(false)
	, bMouseInsideMarquee(false)
	, LastOptionsFlags(FCanvasOptionsFlags::None)
	, LastSetPixelColorIndex(UI::EZXSpectrumColor::None)
	, LastSetPixelPosition(-1.0f, -1.0f)
	, Width(0)
	, Height(0)
	, SourcePathFile(_SourcePathFile)
{
	ButtonColor[0] = UI::EZXSpectrumColor::Black_;
	ButtonColor[1] = UI::EZXSpectrumColor::Black;

	Subcolor[ESubcolor::Ink] = UI::EZXSpectrumColor::Black_;
	Subcolor[ESubcolor::Paper] = UI::EZXSpectrumColor::White_;
	Subcolor[ESubcolor::Bright] = UI::EZXSpectrumColor::False;
	Subcolor[ESubcolor::Flash] = UI::EZXSpectrumColor::False;

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

void SCanvas::Initialize(const std::any& Arg)
{
	if (Arg.type() == typeid(std::filesystem::path))
	{
		const std::filesystem::path FilePath = std::any_cast<std::filesystem::path>(Arg);

		ZXColorView = std::make_shared<UI::FZXColorView>();

		ZXColorView->Scale = ImVec2(2.5f, 2.5f);
		ZXColorView->ImagePosition = ImVec2(0.0f, 0.0f);

		uint8_t* ImageData = FImageBase::LoadToMemory(FilePath, Width, Height);
		UI::QuantizeToZX(ImageData, Width, Height, 4, ZXColorView->IndexedData);
		UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);
		ConversionToZX(ConversationSettings);
		FImageBase::ReleaseLoadedIntoMemory(ImageData);

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
	else if (Arg.type() == typeid(ImVec2))
	{
		const ImVec2 CanvasSize = std::any_cast<ImVec2>(Arg);

		ZXColorView = std::make_shared<UI::FZXColorView>();
		ZXColorView->Scale = ImVec2(2.5f, 2.5f);
		ZXColorView->ImagePosition = ImVec2(0.0f, 0.0f);

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
		DestroyWindow();
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
		const float WidthSource = 25.0f;
		const float WidthConvert = 25.0f;
		const float WidthInk = 25.0f;
		const float WidthPaper = 25.0f;
		const float WidthMask = 25.0f;
		const float Spacing = ImGui::GetStyle().ItemSpacing.x;
		const float TotalWidth = WidthSource + WidthConvert + WidthInk + WidthPaper + WidthMask + Spacing * 4.0f;
		const float StartButtons = WindowWidth - TotalWidth;

		const bool bSourceEnabled = ZXColorView->IndexedData.size() > 0;
		const bool bConvertEnabled = bNeedConvertCanvasToZX || bNeedConvertZXToCanvas;
		const bool bInkEnabled = ZXColorView->InkData.size() > 0;
		const bool bPaperEnabled = ZXColorView->AttributeData.size() > 0;
		const bool bMaskEnabled = ZXColorView->MaskData.size() > 0;

		ImGui::SameLine(StartButtons);
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
							const int8_t Color = UI::EZXSpectrumColor::Black_;
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
			FEvent_Sprite Event;
			Event.Tag = FEventTag::AddSpriteTag;
			Event.Width = Width;
			Event.Height = Height;
			Event.SpriteRect = ImRect(ZXColorView->RectangleMarqueeRect.Min, ZXColorView->RectangleMarqueeRect.Min + CreateSpriteSize);
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

	if (ImGui::IsKeyPressed(ImGuiKey_ModAlt) && !IsEqualToolMode(EToolMode::Eyedropper))
	{
		SetToolMode(EToolMode::Eyedropper, false);
	}
	else if (ImGui::IsKeyReleased(ImGuiKey_ModAlt)/* && !IsEqualToolMode(EToolMode::None, 1)*/)
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
			UI::QuantizeToZX(ClipboardImage.Data.data(), Width, Height, 4, ZXColorView->IndexedData);
			UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);
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
		return;
	}

	if (ZXColorView->bVisibilityRectangleMarquee && (!ZXColorView->bVisibilityRectangleMarquee || !bMouseInsideMarquee))
	{
		return;
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
			Subcolor[ESubcolor::Ink] = (UI::EZXSpectrumColor::Type)AttributeInkColor;
			break;
		case FCanvasOptionsFlags::Attribute:														// 0100
		case FCanvasOptionsFlags::Ink | FCanvasOptionsFlags::Attribute:								// 0110
			Subcolor[ESubcolor::Ink] = (UI::EZXSpectrumColor::Type)AttributeInkColor;
			Subcolor[ESubcolor::Paper] = (UI::EZXSpectrumColor::Type)AttributePaperColor;
			Subcolor[ESubcolor::Bright] = bAttributeBright ? UI::EZXSpectrumColor::True : UI::EZXSpectrumColor::False;
			break;
		case FCanvasOptionsFlags::Mask:																// 1000
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Ink:									// 1010
			Subcolor[ESubcolor::Ink] = (UI::EZXSpectrumColor::Type)AttributeInkColor;
			break;
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Attribute:							// 1100
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Attribute | FCanvasOptionsFlags::Ink:	// 1110
			Subcolor[ESubcolor::Ink] = (UI::EZXSpectrumColor::Type)AttributeInkColor;
			Subcolor[ESubcolor::Paper] = (UI::EZXSpectrumColor::Type)AttributePaperColor;
			Subcolor[ESubcolor::Bright] = bAttributeBright ? UI::EZXSpectrumColor::True : UI::EZXSpectrumColor::False;
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

	if (OptionsFlags[0] & FCanvasOptionsFlags::Source)
	{
		const uint32_t Offset = (uint32_t)Position.y * Width + (uint32_t)Position.x;
		ZXColorView->IndexedData[Offset] = ColorIndex;
		bNeedConvertCanvasToZX = true;
	}
	else
	{
		const int32_t Boundary_X = Width >> 3;
		const int32_t Boundary_Y = Height >> 3;

		const int32_t x = (uint32_t)Position.x;
		const int32_t y = (uint32_t)Position.y;

		const int32_t bx = x / 8;
		const int32_t dx = x % 8;
		const int32_t by = y / 8;
		const int32_t dy = y % 8;

		const uint8_t Flags = OptionsFlags[0] & ~FCanvasOptionsFlags::Source;

		const int32_t InkMaskOffset = (by * 8 + dy) * Boundary_X + bx;
		uint8_t& Pixels = ZXColorView->InkData[InkMaskOffset];
		uint8_t& Mask = ZXColorView->MaskData[InkMaskOffset];

		const int32_t AttributeOffset = by * Boundary_X + bx;
		uint8_t& Attribute = ZXColorView->AttributeData[AttributeOffset];

		const bool bAttributeBright = (Attribute >> 6) & 0x01;
		const uint8_t AttributeInkColor = (Attribute & 0x07);
		const uint8_t AttributePaperColor = ((Attribute >> 3) & 0x07);

		const uint8_t PixelBit = 1 << (7 - dx);

		auto ApplyPixel = [&]()
			{
				if (ColorIndex == EZXColor::Transparent)
				{
					return;
				}
				const bool bOperation = (ColorIndex & 0x07) != UI::EZXSpectrumColor::White;
				if (bOperation)
				{
					Pixels |= PixelBit;									// set bit
				}
				else
				{
					Pixels &= ~(PixelBit);								// reset bit
				}
			};
		auto ApplyAttribute = [&]()
			{
				const bool bInkTransparent = Subcolor[ESubcolor::Ink] == UI::EZXSpectrumColor::Transparent;
				const bool bPaperTransparent = Subcolor[ESubcolor::Paper] == UI::EZXSpectrumColor::Transparent;

				const uint8_t InkColor = bInkTransparent ? AttributeInkColor : Subcolor[ESubcolor::Ink] & 0x07;
				const uint8_t PaperColor = bPaperTransparent ? AttributePaperColor : Subcolor[ESubcolor::Paper] & 0x07;
				const bool bBright = Subcolor[ESubcolor::Bright] == UI::EZXSpectrumColor::True;
				const bool bFlash = Subcolor[ESubcolor::Flash] == UI::EZXSpectrumColor::True;

				const uint8_t AttributeColor = (bFlash << 7) | (bBright << 6) | (PaperColor << 3) | InkColor;
				Attribute = AttributeColor;
			};
		auto ApplyMask = [&]()
			{
				const bool bOperation = ColorIndex != UI::EZXSpectrumColor::Transparent;
				if (bOperation)
				{
					Mask |= PixelBit;									// set bit
				}
				else
				{
					Mask &= ~(PixelBit);								// reset bit
				}
			};

		if (Flags & FCanvasOptionsFlags::Ink)		{ ApplyPixel(); }
		if (Flags & FCanvasOptionsFlags::Attribute) { ApplyAttribute(); }
		if (Flags & FCanvasOptionsFlags::Mask)		{ ApplyMask(); }

		const bool bInk = OptionsFlags[0] & FCanvasOptionsFlags::Ink;
		const bool bMask = OptionsFlags[0] & FCanvasOptionsFlags::Mask;
		const bool bPaper = OptionsFlags[0] & FCanvasOptionsFlags::Attribute;

		bNeedConvertZXToCanvas = true;
	}
	bRefreshCanvas = true;
}

void SCanvas::UpdateCursorColor(bool bButton /*= false*/)
{
	if (bButton)
	{
		ZXColorView->CursorColor = UI::ToVec4(UI::ZXSpectrumColorRGBA[ButtonColor[0]]);
	}
	else
	{
		const bool bBright = Subcolor[ESubcolor::Bright] == UI::EZXSpectrumColor::True;
		const bool bFlash = Subcolor[ESubcolor::Flash] == UI::EZXSpectrumColor::True;
		const uint8_t InkColor = Subcolor[ESubcolor::Ink] == UI::EZXSpectrumColor::Transparent ? UI::EZXSpectrumColor::Transparent : Subcolor[ESubcolor::Ink] | (bBright << 3);
		const uint8_t PaperColor = Subcolor[ESubcolor::Paper] == UI::EZXSpectrumColor::Transparent ? UI::EZXSpectrumColor::Transparent : Subcolor[ESubcolor::Paper] | (bBright << 3);

		ZXColorView->CursorColor = UI::ToVec4(UI::ZXSpectrumColorRGBA[InkColor]);
	}
}
