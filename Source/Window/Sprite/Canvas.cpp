#include "Canvas.h"
#include "Utils/UI/Draw.h"
#include "Events.h"
#include "Utils/Hotkey.h"


namespace
{
	static const char* ThisWindowName = TEXT("Canvas");
}

SCanvas::SCanvas(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
	, bDragging(false)
	, bRefreshCanvas(false)
	, bNeedConvertCanvasToZX(false)
	, bNeedConvertZXToCanvas(false)
	, LastOptionsFlags(FCanvasOptionsFlags::None)
	, LastSetPixelColorIndex(UI::EZXSpectrumColor::None)
	, LastSetPixelPosition(-1.0f, -1.0f)
	, Width(0)
	, Height(0)
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
}

void SCanvas::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_PaletteBar>(
		[this](const FEvent_PaletteBar& Event)
		{
			if (Event.Tag == FEventTag::SelectedColorTag)
			{
				ButtonColor[Event.SelectedColor.ButtonIndex & 0x01] = Event.SelectedColor.SelectedColorIndex;
				if (Event.SelectedColor.SelectedSubcolorIndex < ESubcolor::MAX)
				{
					Subcolor[Event.SelectedColor.SelectedSubcolorIndex] = Event.SelectedColor.SelectedColorIndex;
				}
				UpdateCursorColor();
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
}

void SCanvas::Initialize()
{
	ZXColorView = std::make_shared<UI::FZXColorView>();

	ZXColorView->Scale = ImVec2(2.5f, 2.5f);
	ZXColorView->ImagePosition = ImVec2(0.0f, 0.0f);

	//uint8_t* ImageData = FImageBase::LoadToMemory("D:\\Work\\[Sprite]\\Геройчики\\Fake\\111.png", Width, Height);
	uint8_t* ImageData = FImageBase::LoadToMemory("D:\\Work\\[Sprite]\\Геройчики\\Tile\\Hex\\Hex1.png", Width, Height);
	UI::QuantizeToZX(ImageData, Width, Height, 4, ZXColorView->IndexedData);
	UI::ZXIndexColorToRGBA(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);

	{
		FEvent_StatusBar Event;
		Event.Tag = FEventTag::CanvasSizeTag;
		Event.CanvasSize = ImVec2((float)Width, (float)Height);
		SendEvent(Event);
	}

	ConversionToZX(ConversationSettings);

	FImageBase::ReleaseLoadedIntoMemory(ImageData);

	ZXColorView->Device = Data.Device;
	ZXColorView->DeviceContext = Data.DeviceContext;
	Draw_ZXColorView_Initialize(ZXColorView, UI::ERenderType::Screen);
}

void SCanvas::Render()
{
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
			UI::ZXIndexColorToRGBA(ZXColorView->Image, ZXColorView->IndexedData, Width, Height);
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

	ImGui::Begin(ThisWindowName, &bOpen);
	{
		Input_HotKeys();
		Input_Mouse();
		ApplyToolMode();

		ImGui::Spacing();
		if (ImGui::Button("Options", { 0.0f, 25.0f }))
		{
			ImGui::OpenPopup("Context");
		}
		if (ImGui::BeginPopup("Context"))
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
}

void SCanvas::Input_HotKeys()
{
	static std::vector<FHotKey> Hotkeys =
	{
		{ ImGuiKey_Escape,						ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_None,				this)	},	// 
		{ ImGuiKey_M,							ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_RectangleMarquee,	this)	},	// 
		{ ImGuiKey_B,							ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_Pencil,				this)	},	// 
		{ ImGuiKey_E,							ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_Eraser,				this)	},	// 
		{ ImGuiKey_G,							ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_PaintBucket,		this)	},	// 
		{ ImGuiKey_I,							ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetToolMode_Eyedropper,			this)	},	//
		//{ ImGuiMod_Alt | ImGuiKey_MouseLeft,	ImGuiInputFlags_Repeat,	std::bind(&ThisClass::Imput_SetMode_Eyedropper, this)			},	// (alt + left mouse button)
	};

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

	switch (ToolMode[0])
	{
	case EToolMode::None:
		ZXColorView->bCursorEnable = false;
		break;
	case EToolMode::RectangleMarquee:
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

void SCanvas::Handler_Pencil()
{
	ImGuiContext& Context = *ImGui::GetCurrentContext();
	if (!Context.IO.MouseDown[ImGuiMouseButton_Left] && !Context.IO.MouseDown[ImGuiMouseButton_Right])
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

		FEvent_Canvas Event;
		Event.Tag = FEventTag::SelectedColorTag;
		FSelectedColor SelectedColor
		{
			.ButtonIndex = ButtonIndex,
			.SelectedColorIndex = ButtonColor[ButtonIndex],
			.SelectedSubcolorIndex = (ESubcolor::Type)ButtonIndex,
		};
		Event.SelectedColor = SelectedColor;
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
			break;
		case FCanvasOptionsFlags::Attribute:														// 0100
			break;
		case FCanvasOptionsFlags::Ink | FCanvasOptionsFlags::Attribute:								// 0110
			break;
		case FCanvasOptionsFlags::Mask:																// 1000
			break;
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Ink:									// 1010
			break;
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Attribute:							// 1100
			break;
		case FCanvasOptionsFlags::Mask | FCanvasOptionsFlags::Attribute | FCanvasOptionsFlags::Ink:	// 1110
			break;
		}
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
	UI::ZXIndexColorToRGBA(
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
				const bool bOperation = (ColorIndex & 0x07) == UI::EZXSpectrumColor::Black /*|| ButtonIndex != 0*/;
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
				const bool bOperation = ColorIndex == UI::EZXSpectrumColor::Transparent/* || ButtonIndex == 0*/;
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
