#include "Canvas.h"
#include "Timeline.h"
#include "Keyframes.h"
#include "SpriteList.h"
#include <AppSprite.h>
#include <Utils/UI/Draw.h>
#include <Window/Sprite/Events.h>
#include <Utils/IO.h>
#include <Utils/Aseprite/Format.h>
#include <Utils/6912/CodeGenerator.h>

#include "stb/stb_image_write.h"

struct FZXDataDiff
{
	enum class EState
	{
		Equal,
		Different,
		Missing,
		SizeMismatch,
		ReadError,
	};
	EState State = EState::Missing;
	std::vector<uint8_t> SavedData;
	std::vector<uint8_t> SourceData;
	size_t DifferentBytes = 0;
	std::error_code Error;
};

struct FSourceZXDiff
{
	enum class EOperation
	{
		And,
		Or,
		Xor,
	};
	int32_t Width = 0;
	int32_t Height = 0;
	int32_t Frame = INDEX_NONE;
	FZXDataDiff Ink;
	FZXDataDiff Attribute;
	FZXDataDiff Mask;
	// Default operations for the three rows of the source comparison dialog.
	EOperation InkOperation = EOperation::And;
	EOperation AttributeOperation = EOperation::Or;
	EOperation MaskOperation = EOperation::Or;
	bool bPreview = true;
	bool bPreviewDirty = true;
	std::vector<uint8_t> PreviewInk;
	std::vector<uint8_t> PreviewAttribute;
	std::vector<uint8_t> PreviewMask;

	void UpdatePreviewData()
	{
		// Check whether the selected operations require a new temporary result.
		if (!bPreviewDirty)
		{
			return;
		}
		auto MergeData = [](const FZXDataDiff& Diff, EOperation Operation, std::vector<uint8_t>& Output) -> void
		{
			// Create a result for the same byte positions as the two original versions.
			Output.resize(Diff.SourceData.size());
			// Calculate each result byte without accumulating previous previews.
			for (size_t Index = 0; Index < Output.size(); ++Index)
			{
				switch (Operation)
				{
				case EOperation::And:
					Output[Index] = Diff.SavedData[Index] & Diff.SourceData[Index];
					break;
				case EOperation::Or:
					Output[Index] = Diff.SavedData[Index] | Diff.SourceData[Index];
					break;
				case EOperation::Xor:
					Output[Index] = Diff.SavedData[Index] ^ Diff.SourceData[Index];
					break;
				}
			}
		};
		// Build the three components for both Preview and Apply.
		MergeData(Ink, InkOperation, PreviewInk);
		MergeData(Attribute, AttributeOperation, PreviewAttribute);
		MergeData(Mask, MaskOperation, PreviewMask);
		bPreviewDirty = false;
	}
};

struct FSourceZXApplyAction : Undo::FContinuousMarkerAction
{
	std::vector<uint8_t> InkData;
	std::vector<uint8_t> AttributeData;
	std::vector<uint8_t> MaskData;
	bool bNeedConvertZXToCanvas = true;
	std::function<void(FSourceZXApplyAction&)> Swap;

	void Execute() override
	{
		Swap(*this);
	}
};

namespace
{
	static const wchar_t* ThisWindowName = L"Canvas";
	static const char* PopupMenuName = TEXT("##PopupMenuSprite");
	static const char* CreateSpriteName = "##CreateSprite";
	std::weak_ptr<SCanvas> ActiveCanvas;

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

	int32_t GetZXScreenPixelOffset(int32_t LinearPixelByteOffset)
	{
		const int32_t ByteY = LinearPixelByteOffset / 32;
		const int32_t ByteX = LinearPixelByteOffset % 32;
		return ((ByteY & 0xC0) << 5) | ((ByteY & 0x07) << 8) | ((ByteY & 0x38) << 2) | ByteX;
	}

	void ConvertZXDataToIndexed(
		int32_t Width,
		int32_t Height,
		const std::vector<uint8_t>& InkData,
		const std::vector<uint8_t>& AttributeData,
		const std::vector<uint8_t>& MaskData,
		std::vector<uint8_t>& OutputIndexedData)
	{
		const int32_t BoundaryX = Width >> 3;
		OutputIndexedData.resize(static_cast<size_t>(Width) * Height);
		for (int32_t Y = 0; Y < Height; ++Y)
		{
			for (int32_t X = 0; X < Width; ++X)
			{
				const int32_t ByteX = X >> 3;
				const uint8_t PixelBit = static_cast<uint8_t>(1 << (7 - (X & 7)));
				const int32_t PixelIndex = Y * BoundaryX + ByteX;
				const int32_t AttributeIndex = (Y >> 3) * BoundaryX + ByteX;
				const int32_t OutputIndex = Y * Width + X;
				if (PixelIndex < 0 ||
					PixelIndex >= static_cast<int32_t>(InkData.size()) ||
					PixelIndex >= static_cast<int32_t>(MaskData.size()) ||
					AttributeIndex < 0 ||
					AttributeIndex >= static_cast<int32_t>(AttributeData.size()) ||
					(MaskData[PixelIndex] & PixelBit) == 0)
				{
					OutputIndexedData[OutputIndex] = EZXColor::Transparent;
					continue;
				}

				const uint8_t Attribute = AttributeData[AttributeIndex];
				const uint8_t Bright = (Attribute >> 6) & 0x01;
				const uint8_t Ink = (Attribute & 0x07) | (Bright << 3);
				const uint8_t Paper = ((Attribute >> 3) & 0x07) | (Bright << 3);
				OutputIndexedData[OutputIndex] = (InkData[PixelIndex] & PixelBit) != 0 ? Ink : Paper;
			}
		}
	}

	ImRect AlignLimitAreaToAttributeCells(const ImRect& Rect, int32_t Width, int32_t Height)
	{
		ImRect Result;
		Result.Min.x = floorf(Rect.Min.x / 8.0f) * 8.0f;
		Result.Min.y = floorf(Rect.Min.y / 8.0f) * 8.0f;
		Result.Max.x = ceilf(Rect.Max.x / 8.0f) * 8.0f;
		Result.Max.y = ceilf(Rect.Max.y / 8.0f) * 8.0f;
		Result.Min = ImClamp(Result.Min, ImVec2(0.0f, 0.0f), ImVec2((float)Width, (float)Height));
		Result.Max = ImClamp(Result.Max, ImVec2(0.0f, 0.0f), ImVec2((float)Width, (float)Height));
		return Result;
	}

	void ApplyLimitAreaToCodeGenerationMask(
		const ImRect& Rect,
		int32_t Width,
		int32_t Height,
		std::vector<uint8_t>& PixelAllowedMask,
		std::vector<uint8_t>& AttributeAllowedMask)
	{
		const ImRect Area = AlignLimitAreaToAttributeCells(Rect, Width, Height);
		if (Area.GetWidth() <= 0.0f || Area.GetHeight() <= 0.0f)
		{
			return;
		}

		const int32_t Boundary_X = Width >> 3;
		const int32_t MinByteX = (int32_t)Area.Min.x >> 3;
		const int32_t MaxByteX = (int32_t)Area.Max.x >> 3;
		const int32_t MinY = (int32_t)Area.Min.y;
		const int32_t MaxY = (int32_t)Area.Max.y;

		for (int32_t y = MinY; y < MaxY; ++y)
		{
			for (int32_t ByteX = MinByteX; ByteX < MaxByteX; ++ByteX)
			{
				const int32_t PixelIndex = y * Boundary_X + ByteX;
				if (PixelIndex >= 0 && PixelIndex < (int32_t)PixelAllowedMask.size())
				{
					PixelAllowedMask[PixelIndex] = 1;
				}
			}
		}

		const int32_t MinAttrY = MinY >> 3;
		const int32_t MaxAttrY = MaxY >> 3;
		for (int32_t AttrY = MinAttrY; AttrY < MaxAttrY; ++AttrY)
		{
			for (int32_t ByteX = MinByteX; ByteX < MaxByteX; ++ByteX)
			{
				const int32_t AttrIndex = AttrY * Boundary_X + ByteX;
				if (AttrIndex >= 0 && AttrIndex < (int32_t)AttributeAllowedMask.size())
				{
					AttributeAllowedMask[AttrIndex] = 1;
				}
			}
		}
	}

	void ApplyActiveAreaToCodeGenerationMask(
		const ImRect& Rect,
		int32_t Width,
		int32_t Height,
		const FTilemapCellData_ByteValues& IgnoredPixels,
		const std::vector<uint8_t>& InkData,
		const std::vector<uint8_t>& AttributeData,
		const std::vector<uint8_t>& MaskData,
		std::vector<uint8_t>& PixelAllowedMask,
		std::vector<uint8_t>& AttributeAllowedMask)
	{
		const ImRect Area = AlignLimitAreaToAttributeCells(Rect, Width, Height);
		if (Area.GetWidth() <= 0.0f || Area.GetHeight() <= 0.0f)
		{
			return;
		}

		const int32_t Boundary_X = Width >> 3;
		const int32_t MinByteX = (int32_t)Area.Min.x >> 3;
		const int32_t MaxByteX = (int32_t)Area.Max.x >> 3;
		const int32_t MinY = (int32_t)Area.Min.y;
		const int32_t MaxY = (int32_t)Area.Max.y;

		for (int32_t y = MinY; y < MaxY; ++y)
		{
			for (int32_t ByteX = MinByteX; ByteX < MaxByteX; ++ByteX)
			{
				const int32_t PixelIndex = y * Boundary_X + ByteX;
				if (PixelIndex < 0 || PixelIndex >= (int32_t)PixelAllowedMask.size() || PixelIndex >= (int32_t)InkData.size() || PixelIndex >= (int32_t)MaskData.size())
				{
					continue;
				}

				const uint8_t Mask = MaskData[PixelIndex];
				const uint8_t Pixels = InkData[PixelIndex];
				if (Mask == 0x00 || IgnoredPixels.Contains(Pixels))
				{
					continue;
				}

				PixelAllowedMask[PixelIndex] = 1;

				const int32_t AttrIndex = (y >> 3) * Boundary_X + ByteX;
				if (AttrIndex >= 0 && AttrIndex < (int32_t)AttributeAllowedMask.size() && AttrIndex < (int32_t)AttributeData.size())
				{
					AttributeAllowedMask[AttrIndex] = 1;
				}
			}
		}
	}
}

int32_t SCanvas::SpriteCounter = 0;
std::string SCanvas::LastSelectedSpriteNameBase;

SCanvas::SCanvas(EFont::Type _FontName, const std::wstring& Name, const std::filesystem::path& _SourcePathFile)
	: Super(FWindowInitializer()
		.SetName(std::format(L"{}##{}", Name.c_str(), ThisWindowName))
		.SetFontName(_FontName)
		.SetDockSlot("##Layout_Canvas")
		.SetIncludeInWindows(true))
	, bPlay(false)
	, bSourceDirty(false)
	, bAsepriteSourceDirty(false)
	, bIPMDirty(false)
	, bDragging(false)
	, bRefreshCanvas(false)
	, bTransparentMask(false)
	, bAttributeClash(false)
	, bLastAttributeClashPhase(false)
	, bRectangleMarqueeActive(false)
	, bNeedConvertCanvasToZX(false)
	, bNeedConvertZXToCanvas(false)
	, bOpenPopupMenu(false)
	, bMouseInsideMarquee(false)
	, bFroceRebuiltSpriteFrame(false)
	, bOpenFrameInversionPopup(false)
	, bInvertFramePixels(true)
	, bInvertFrameAttributes(true)
	, bInvertAllFrames(false)
	, bCheckSourceZXDiff(false)
	, ZXDataGeneration(0)
	, bSourceZXPreviewActive(false)
	, LastOptionsFlags(FCanvasOptionsFlags::None)
	, LastSetPixelColorIndex(EZXColor::None)
	, LastSetPixelPosition(-1.0f, -1.0f)
	, PlayDuration(0.0f)
	, Width(0)
	, Height(0)
	, SelectedSpritesFrame(0)
	, LastRebuiltSpriteFrame(INDEX_NONE)
	, MaxFramesInSprites(0)
	, FrameMode(EFrameMode::None)
	, ImageFormat(EImageFormat::None)
	, ImageFrameIndex(INDEX_NONE)
	, SourcePathFile(_SourcePathFile)
	, CreateSpriteNameBuffer{}
	, CreateSpriteWidthBuffer{}
	, CreateSpriteHeightBuffer{}
{
	ButtonColor[0] = EZXColor::Black_;
	ButtonColor[1] = EZXColor::Black;

	Subcolor[ESubcolor::Ink] = EZXColor::Black_;
	Subcolor[ESubcolor::Paper] = EZXColor::White_;
	Subcolor[ESubcolor::Bright] = EZXColor::False;
	Subcolor[ESubcolor::Flash] = EZXColor::False;

	OptionsFlags[0] = FCanvasOptionsFlags::Source;
	OptionsFlags[1] = FCanvasOptionsFlags::Source;

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

	SubscribeEvent<FEvent_Canvas>(
		[this](const FEvent_Canvas& Event)
		{
			if (!Event.CanvasName.empty() && GetWindowWName() == Event.CanvasName)
			{
				return;
			}

			if (Event.Tag == FEventTag::CanvasOptionsFlagsTag)
			{
				OptionsFlags[0] = Event.OptionsFlags;
				OptionsFlags[1] = Event.OptionsFlags;
			}
			else if (Event.Tag == FEventTag::CanvasViewFlagsTag)
			{
				const bool bAttributeClashChanged = bAttributeClash != Event.ViewFlags.bAttributeClash;
				bTransparentMask = Event.ViewFlags.bTransparentMask;
				bAttributeClash = Event.ViewFlags.bAttributeClash;
				ZXColorView->Options.bGrid = Event.ViewFlags.bGrid;
				ZXColorView->Options.bPixelGrid = Event.ViewFlags.bPixelGrid;
				ZXColorView->Options.bAttributeGrid = Event.ViewFlags.bAttributeGrid;
				ZXColorView->Options.bAlphaCheckerboardGrid = Event.ViewFlags.bAlphaCheckerboardGrid;
				ZXColorView->Options.GridSettingSize = Event.ViewFlags.GridSettingSize;
				ZXColorView->Options.GridSettingOffset = Event.ViewFlags.GridSettingOffset;
				
				ZXColorView->TransparentColor = Event.ViewFlags.TransparentColor;
				bTransparentMask = Event.ViewFlags.bTransparentMask;

				ChangeFrameMode(Event.ViewFlags.FrameMode);
				bRefreshCanvas |= bAttributeClashChanged;
			}
			else if (Event.Tag == FEventTag::CanvasViewScaleTag)
			{
				if (Event.CanvasWidth != Width || Event.CanvasHeight != Height)
				{
					return;
				}
				UI::Set_ZXViewScale(ZXColorView, Event.MouseWheel);
			}
			else if (Event.Tag == FEventTag::CanvasViewPositionTag)
			{
				if (Event.CanvasWidth != Width || Event.CanvasHeight != Height)
				{
					return;
				}
				UI::Add_ZXViewDeltaPosition(ZXColorView, Event.ImagePosition);
			}

			bRefreshCanvas = true;
		});

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

	SubscribeEvent<FEvent_Timeline>(
		[this](const FEvent_Timeline& Event)
		{
			if (Event.Tag == FEventTag::TimelineLayerVisibilityChangedTag &&
				Event.Sprite == AsepriteSprite)
			{
				bFroceRebuiltSpriteFrame = true;
				bRefreshCanvas = true;
			}
			else if (Event.Tag == FEventTag::TimelineLayerAssignmentChangedTag &&
				Event.Sprite == AsepriteSprite)
			{
				for (int32_t Frame = 0; Frame < static_cast<int32_t>(AsepriteSprite->Frames.size()); ++Frame)
				{
					std::vector<uint8_t> InkData;
					std::vector<uint8_t> AttributeData;
					std::vector<uint8_t> MaskData;
					if (!BuildAsepriteFrameZXData(Frame, InkData, AttributeData, MaskData))
						continue;

					std::vector<uint8_t> IndexedData;
					ConvertZXDataToIndexed(
						Width,
						Height,
						InkData,
						AttributeData,
						MaskData,
						IndexedData);
					NotifySpritesUpdated(Frame, IndexedData, InkData, AttributeData, MaskData);
				}
				LastRebuiltSpriteFrame = INDEX_NONE;
				bFroceRebuiltSpriteFrame = true;
				bRefreshCanvas = true;
			}
		});

	SubscribeEvent<FEvent_SelectedSprite>(
		[this](const FEvent_SelectedSprite& Event)
		{
			if (Event.Tag == FEventTag::SelectedSpritesChangedTag)
			{
				if (!Event.Sprite)
				{
					SelectedSprite.reset();
					ZXColorView->bVisibilityRectangleMarquee = false;
					return;
				}

				const std::filesystem::path CanvasSourcePath = SourcePathFile.empty()
					? std::filesystem::path(GetWindowWName())
					: SourcePathFile;
				if (CanvasSourcePath != Event.Sprite->SourcePathFile)
				{
					return;
				}
				// Check whether the selected sprite belongs to the loaded Aseprite source.
				if (ImageFormat == EImageFormat::Aseprite && AsepriteSprite)
				{
					// Calculate the frame restored for the selected sprite.
					int32_t SpriteFrame = 0;
					// Check whether the stored frame belongs to the loaded Aseprite source.
					if (Event.Sprite->AsepriteIndex >= 0 && Event.Sprite->AsepriteIndex <= MaxFramesInSprites)
					{
						SpriteFrame = Event.Sprite->AsepriteIndex;
					}

					// Calculate the source layer restored for the selected sprite.
					const int32_t LayerCount = static_cast<int32_t>(AsepriteSprite->Layers.size());
					int32_t SpriteLayer = 0;
					// Check whether the stored layer belongs to the loaded Aseprite source.
					if (Event.Sprite->LayerIndex >= 0 && Event.Sprite->LayerIndex < LayerCount)
					{
						SpriteLayer = Event.Sprite->LayerIndex;
					}

					// Check that pending frame edits can be saved before changing the selected sprite source.
					if (SpriteFrame != SelectedSpritesFrame && !PrepareToChangeFrame())
					{
						return;
					}

					// Apply the stored source frame before refreshing the selected sprite.
					SelectedSpritesFrame = SpriteFrame;
					bRefreshCanvas = true;

					// Restore the frame and source layer stored by the selected sprite.
					FEvent_Timeline SourceSelectionEvent(FEventTag::TimelineInitializeTag);
					SourceSelectionEvent.Keyframes = Keyframes;
					SourceSelectionEvent.Sprite = AsepriteSprite;
					SourceSelectionEvent.Format = ImageFormat;
					SourceSelectionEvent.Frame = SpriteFrame;
					SourceSelectionEvent.LayerIndex = SpriteLayer;
					SendEvent(SourceSelectionEvent);
				}

				// Focus the Canvas that owns the selected sprite after its source state is restored.
				Focus();
				SelectedSprite = Event.Sprite;
				bPlay = false;
				PlayDuration = 0.0f;
				ZXColorView->bVisibilityRectangleMarquee = true;
				ZXColorView->RectangleMarqueeRect.Min = ImVec2((float)SelectedSprite->SpritePositionToImageX, (float)SelectedSprite->SpritePositionToImageY);
				ZXColorView->RectangleMarqueeRect.Max = ImVec2((float)SelectedSprite->SpritePositionToImageX + (float)SelectedSprite->Width, (float)SelectedSprite->SpritePositionToImageY + (float)SelectedSprite->Height);
				// ToDo: screen space rect
			}
			else if (Event.Tag == FEventTag::SelectedSpritesChangedFrameTag)
			{
				if (ActiveCanvas.lock().get() != this ||
					Event.Format != ImageFormat ||
					Event.Frame < 0 ||
					Event.Frame > MaxFramesInSprites)
				{
					return;
				}

				if (Event.Frame != SelectedSpritesFrame && !PrepareToChangeFrame())
				{
					return;
				}
				SelectedSpritesFrame = Event.Frame;
				bRefreshCanvas = true;
			}
		});

	SubscribeEvent<FEvent_Sprite>(
		[this](const FEvent_Sprite& Event)
		{
			if (Event.Tag == FEventTag::RenamedSpriteTag)
			{
				SpriteNames[Event.UniqueID] = Event.SpriteName;
			}
			else if (Event.Tag == FEventTag::ResponseAllSpritesTag)
			{
				SpriteNames[Event.UniqueID] = Event.SpriteName;
			}
		});
}

void SCanvas::Initialize(const std::vector<std::any>& Args)
{
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
		else if (Arg.type() == typeid(std::shared_ptr<AsepriteFormat::FSprite>))
		{
			AsepriteSprite = std::any_cast<std::shared_ptr<AsepriteFormat::FSprite>>(Arg);
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
		Width = AsepriteSprite->Width;
		Height = AsepriteSprite->Height;
		Keyframes = std::make_shared<FKeyframes>();
		Keyframes->Make((int32_t)AsepriteSprite->Frames.size(), (int32_t)AsepriteSprite->Layers.size());
		TransparentColor = UI::ToU32(COLOR(0, 0, 0, 0));

		FEvent_Timeline Timeline_Event(FEventTag::TimelineInitializeTag);
		{
			Timeline_Event.Keyframes = Keyframes;
			Timeline_Event.Sprite = AsepriteSprite;
			Timeline_Event.Format = EImageFormat::Aseprite;
			Timeline_Event.Frame = 0;
			Timeline_Event.LayerIndex = INDEX_NONE;
			SendEvent(Timeline_Event);
		}

		FEvent_StatusBar Status_Event(FEventTag::CanvasSizeTag);
		{
			Status_Event.CanvasSize = ImVec2((float)Width, (float)Height);
			SendEvent(Status_Event);
		}

		FEvent_AppSprite AppSprite_Event(FEventTag::NotificationAddCanvasTag);
		{
			AppSprite_Event.Canvas = shared_from_this();
			SendEvent(AppSprite_Event);
		}

		if (AsepriteSprite->Frames.empty())
		{
			LOG_ERROR("[{}]\t *.aseprite file does not have frames.", (__FUNCTION__));
			break;
		}

		ZXColorView->Device = Data.Device;
		ZXColorView->DeviceContext = Data.DeviceContext;
		Draw_ZXColorView_Initialize(ZXColorView, UI::ERenderType::Canvas);

		SelectedSpritesFrame = 0;
		MaxFramesInSprites = (int32_t)AsepriteSprite->Frames.size() - 1;

		UI::QuantizeToZX(AsepriteSprite->Frames[0].data(), Width, Height, 4, ZXColorView->IndexedData, TransparentColor);
		UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);
		ConversionToZX(ConversationSettings);
		LoadAsepriteFrameOverride(0, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData);
		ApplyAsepriteLayerOverrides(0, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData);
		LastRebuiltSpriteFrame = 0;

		break;
	}

	//case EImageFormat::Aseprite_Frame:
	//{
	//	Width = Frame.Width;
	//	Height = Frame.Height;
	//	TransparentColor = UI::ToU32(COLOR(0, 0, 0, 0));
	//
	//	UI::QuantizeToZX(Frame.Image.data(), Width, Height, 4, ZXColorView->IndexedData, TransparentColor);
	//	UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);
	//	ConversionToZX(ConversationSettings);
	//
	//	ZXColorView->Device = Data.Device;
	//	ZXColorView->DeviceContext = Data.DeviceContext;
	//	Draw_ZXColorView_Initialize(ZXColorView, UI::ERenderType::Canvas);
	//
	//	{
	//		std::filesystem::path LoadPath = Frame.Path;
	//		std::filesystem::path LoadName = Frame.Name;
	//
	//		Load(LoadPath, LoadName, false);
	//	}
	//
	//	{
	//		FEvent_StatusBar Event;
	//		Event.Tag = FEventTag::CanvasSizeTag;
	//		Event.CanvasSize = ImVec2((float)Width, (float)Height);
	//		SendEvent(Event);
	//	}
	//
	//	break;
	//}

	default:
	{
		LOG_ERROR("[{}]\t Unknown format image.", (__FUNCTION__));
		break;
	}

	}

	// request for receiving current statuses
	{
		// canvas view flags
		FEvent_Canvas Event_Canvas(FEventTag::RequestCanvasViewFlagsTag);
		SendEvent(Event_Canvas);

		// sprite names
		FEvent_Sprite Event_Sprite(FEventTag::RequestAllSpritesTag);
		SendEvent(Event_Sprite);

		// tools
		FEvent_ToolBar Event_ToolBar(FEventTag::RequestToolModeTag);
		SendEvent(Event_ToolBar);
	}
	// Request source comparison after the canvas has finished loading.
	bCheckSourceZXDiff = CanReloadFromSource();
}

bool SCanvas::HasTimeline() const
{
	if (ImageFormat == EImageFormat::GIF)
	{
		return MaxFramesInSprites > 0;
	}

	return ImageFormat == EImageFormat::Aseprite &&
		AsepriteSprite &&
		(AsepriteSprite->Frames.size() > 1 ||
		 AsepriteSprite->Layers.size() > 1);
}

bool SCanvas::CanReloadFromSource() const
{
	return !SourcePathFile.empty() &&
		(ImageFormat == EImageFormat::PNG || ImageFormat == EImageFormat::Aseprite);
}

bool SCanvas::ReloadFromSource()
{
	if (!CanReloadFromSource())
	{
		return false;
	}

	const int32_t PreviousWidth = Width;
	const int32_t PreviousHeight = Height;
	const int32_t PreviousFrame = SelectedSpritesFrame;

	auto LoadOverride = [](const std::filesystem::path& Path, size_t ExpectedSize, std::vector<uint8_t>& Output)
		{
			if (!std::filesystem::exists(Path))
			{
				return;
			}

			std::vector<uint8_t> Data;
			const std::error_code Error = IO::LoadBinaryData(Data, Path);
			if (Error || Data.size() != ExpectedSize)
			{
				LOG_ERROR("[ReloadFromSource] Invalid override file: {}", Path.string());
				return;
			}
			Output = std::move(Data);
		};

	if (ImageFormat == EImageFormat::PNG)
	{
		int32_t NewWidth = 0;
		int32_t NewHeight = 0;
		std::vector<uint8_t> IndexedData;
		std::vector<uint8_t> InkData;
		std::vector<uint8_t> AttributeData;
		std::vector<uint8_t> MaskData;
		// Convert the source into temporary ZX data before loading saved overrides.
		if (!BuildPNGSourceZXData(NewWidth, NewHeight, IndexedData, InkData, AttributeData, MaskData))
		{
			LOG_ERROR("[ReloadFromSource] Failed to reload '{}'.", SourcePathFile.string());
			return false;
		}

		const std::filesystem::path LoadPath = SourcePathFile.parent_path();
		const std::filesystem::path LoadName = SourcePathFile.stem();
		const size_t ExpectedPixelSize = static_cast<size_t>(NewWidth >> 3) * NewHeight;
		const size_t ExpectedAttributeSize = static_cast<size_t>(NewWidth >> 3) * (NewHeight >> 3);
		LoadOverride(IO::NormalizePath(std::filesystem::absolute(LoadPath / std::format("{}.ink", LoadName.string()))), ExpectedPixelSize, InkData);
		LoadOverride(IO::NormalizePath(std::filesystem::absolute(LoadPath / std::format("{}.attr", LoadName.string()))), ExpectedAttributeSize, AttributeData);
		LoadOverride(IO::NormalizePath(std::filesystem::absolute(LoadPath / std::format("{}.mask", LoadName.string()))), ExpectedPixelSize, MaskData);

		Width = NewWidth;
		Height = NewHeight;
		ZXColorView->IndexedData = std::move(IndexedData);
		ZXColorView->InkData = std::move(InkData);
		ZXColorView->AttributeData = std::move(AttributeData);
		ZXColorView->MaskData = std::move(MaskData);
	}
	else
	{
		std::shared_ptr<AsepriteFormat::FSprite> ReloadedSprite = std::make_shared<AsepriteFormat::FSprite>();
		if (!AsepriteFormat::Load(SourcePathFile, *ReloadedSprite) || !ReloadedSprite->IsValid())
		{
			LOG_ERROR("[ReloadFromSource] Failed to reload '{}'.", SourcePathFile.string());
			return false;
		}

		const size_t PreviousFrameCount = AsepriteSprite ? AsepriteSprite->Frames.size() : 0;
		const size_t PreviousLayerCount = AsepriteSprite ? AsepriteSprite->Layers.size() : 0;
		if (AsepriteSprite)
		{
			ReloadedSprite->InkLayer = AsepriteSprite->InkLayer;
			ReloadedSprite->AttributeLayer = AsepriteSprite->AttributeLayer;
			ReloadedSprite->MaskLayer = AsepriteSprite->MaskLayer;
		}

		AsepriteSprite = std::move(ReloadedSprite);
		Width = AsepriteSprite->Width;
		Height = AsepriteSprite->Height;
		MaxFramesInSprites = static_cast<int32_t>(AsepriteSprite->Frames.size()) - 1;
		SelectedSpritesFrame = ImClamp(PreviousFrame, 0, MaxFramesInSprites);
		if (!Keyframes ||
			PreviousFrameCount != AsepriteSprite->Frames.size() ||
			PreviousLayerCount != AsepriteSprite->Layers.size())
		{
			Keyframes = std::make_shared<FKeyframes>();
			Keyframes->Make(
				static_cast<int32_t>(AsepriteSprite->Frames.size()),
				static_cast<int32_t>(AsepriteSprite->Layers.size()));
		}

		UI::QuantizeToZX(AsepriteSprite->Frames[SelectedSpritesFrame].data(), Width, Height, 4, ZXColorView->IndexedData, TransparentColor);
		UI::ZXIndexColorToZXAttributeColor(ZXColorView->IndexedData, Width, Height, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData, ConversationSettings);
		LoadAsepriteFrameOverride(SelectedSpritesFrame, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData);
		ApplyAsepriteLayerOverrides(SelectedSpritesFrame, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData);

		FEvent_Timeline TimelineEvent(FEventTag::TimelineInitializeTag);
		TimelineEvent.Keyframes = Keyframes;
		TimelineEvent.Sprite = AsepriteSprite;
		TimelineEvent.Format = ImageFormat;
		TimelineEvent.Frame = SelectedSpritesFrame;
		TimelineEvent.LayerIndex = INDEX_NONE;
		SendEvent(TimelineEvent);
	}

	const bool bCreateTexture = !ZXColorView->Image.IsValid() ||
		PreviousWidth != Width || PreviousHeight != Height;
	UI::ZXIndexColorToImage(
		ZXColorView->Image,
		ZXColorView->IndexedData,
		Width,
		Height,
		bCreateTexture);

	FEvent_StatusBar StatusEvent(FEventTag::CanvasSizeTag);
	StatusEvent.CanvasSize = ImVec2(static_cast<float>(Width), static_cast<float>(Height));
	SendEvent(StatusEvent);

	if (ImageFormat == EImageFormat::Aseprite)
	{
		for (int32_t Frame = 0; Frame <= MaxFramesInSprites; ++Frame)
		{
			std::vector<uint8_t> InkData;
			std::vector<uint8_t> AttributeData;
			std::vector<uint8_t> MaskData;
			if (!BuildAsepriteFrameZXData(Frame, InkData, AttributeData, MaskData))
			{
				continue;
			}

			std::vector<uint8_t> IndexedData;
			UI::QuantizeToZX(AsepriteSprite->Frames[Frame].data(), Width, Height, 4, IndexedData, TransparentColor);
			NotifySpritesUpdated(Frame, IndexedData, InkData, AttributeData, MaskData);
		}
	}
	else
	{
		NotifySpritesUpdated(ImageFrameIndex, ZXColorView->IndexedData, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData);
	}

	if (UndoQueue.IsContinuous())
	{
		UndoQueue.EndContinuous();
	}
	UndoQueue.Clear();
	bPlay = false;
	bSourceDirty = false;
	bAsepriteSourceDirty = false;
	bIPMDirty = false;
	bNeedConvertCanvasToZX = false;
	bNeedConvertZXToCanvas = false;
	LastRebuiltSpriteFrame = SelectedSpritesFrame;
	bFroceRebuiltSpriteFrame = true;
	bRefreshCanvas = true;

	LOG_DISPLAY("[ReloadFromSource] Reloaded '{}'.", SourcePathFile.string());
	// Request comparison against the source that has just been reloaded.
	bCheckSourceZXDiff = true;
	return true;
}

void SCanvas::SetAsepriteLayerAssignments(
	const std::string& InkLayer,
	const std::string& AttributeLayer,
	const std::string& MaskLayer)
{
	if (!AsepriteSprite || ImageFormat != EImageFormat::Aseprite)
	{
		return;
	}
	AsepriteSprite->InkLayer = InkLayer;
	AsepriteSprite->AttributeLayer = AttributeLayer;
	AsepriteSprite->MaskLayer = MaskLayer;
	LastRebuiltSpriteFrame = INDEX_NONE;
	bFroceRebuiltSpriteFrame = true;
	bRefreshCanvas = true;
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
		{ ImGuiMod_Ctrl | ImGuiKey_S,					ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteGlobal,	std::bind(&ThisClass::Imput_Save,							Self)	},	// (ctrl + S)
		{ ImGuiKey_Comma,								ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteGlobal,	std::bind(&ThisClass::Imput_PreviousFrame,					Self)	},	// (<)
		{ ImGuiKey_Enter,								ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteGlobal,	std::bind(&ThisClass::Imput_Play,							Self)	},	// (Enter)
		{ ImGuiKey_Period,								ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteGlobal,	std::bind(&ThisClass::Imput_NextFrame,						Self)	},	// (>)
	};
}

void SCanvas::Tick(float DeltaTime)
{
	ZXColorView->TimeCounter += DeltaTime;
	const bool bAttributeClashPhase = fmodf(ZXColorView->TimeCounter, 0.5f) >= 0.25f;
	if (bAttributeClash &&
		!(OptionsFlags[0] & FCanvasOptionsFlags::Source) &&
		FrameMode == EFrameMode::None &&
		bAttributeClashPhase != bLastAttributeClashPhase)
	{
		bLastAttributeClashPhase = bAttributeClashPhase;
		bRefreshCanvas = true;
	}
	if (SelectedSprite)
	{
		SelectedSprite->ZXColorView->TimeCounter += DeltaTime;
	}

	if (bPlay)
	{
		PlayDuration -= DeltaTime;
		if (PlayDuration < 0.0f)
		{
			Imput_NextFrame();
		}
	}
}

bool SCanvas::IsActiveCanvas() const
{
	return ActiveCanvas.lock().get() == this;
}

void SCanvas::Render()
{
	SWindow::Render();

	if (!IsOpen())
	{
		Close();
		return;
	}
	if (NeedFocus())
	{
		ImGui::SetNextWindowFocus();
		ResetFocus();
	}

	const bool bInk = OptionsFlags[0] & FCanvasOptionsFlags::Ink;
	const bool bDirty = bSourceDirty || bIPMDirty || bAsepriteSourceDirty;
	const bool bMask = OptionsFlags[0] & FCanvasOptionsFlags::Mask;
	const bool bPaper = OptionsFlags[0] & FCanvasOptionsFlags::Attribute;
	const bool bSource = OptionsFlags[0] & FCanvasOptionsFlags::Source;

	if (bRefreshCanvas)
	{
		RebuildCanvasFromAseprite(SelectedSpritesFrame);
		bRefreshCanvas = false;
		bSourceZXPreviewActive = false;
	}

	const bool bNoMove = ToolMode[0] == EToolMode::RectangleMarquee;
	//const std::string Title = /*bDirty ? "* " + GetWindowName() : */GetWindowName();
	//const std::string UniqueID = Title + "##" + GetWindowName();

	ImGui::Begin(GetWindowName().c_str(), &bOpen, bNoMove ? ImGuiWindowFlags_NoMove : ImGuiWindowFlags_None);
	SetFocus(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
	if (IsFocus())
	{
		std::shared_ptr<SCanvas> Self = std::dynamic_pointer_cast<SCanvas>(shared_from_this());
		std::shared_ptr<SCanvas> PreviousCanvas = ActiveCanvas.lock();
		if (PreviousCanvas != Self)
		{
			if (PreviousCanvas)
			{
				PreviousCanvas->bPlay = false;
				PreviousCanvas->SelectedSpritesFrame = 0;
				PreviousCanvas->PlayDuration = 0.0f;
				PreviousCanvas->bRefreshCanvas = true;
			}

			bPlay = false;
			PlayDuration = 0.0f;
			bRefreshCanvas = true;
			ActiveCanvas = Self;

			if (ImageFormat == EImageFormat::Aseprite)
			{
				FEvent_Timeline TimelineEvent(FEventTag::TimelineInitializeTag);
				TimelineEvent.Keyframes = Keyframes;
				TimelineEvent.Sprite = AsepriteSprite;
				TimelineEvent.Format = ImageFormat;
				TimelineEvent.Frame = SelectedSpritesFrame;
				TimelineEvent.LayerIndex = INDEX_NONE;
				// Apply the layer stored by the selected sprite when its Canvas becomes active.
				if (SelectedSprite && AsepriteSprite)
				{
					TimelineEvent.LayerIndex = 0;
					const int32_t LayerCount = static_cast<int32_t>(AsepriteSprite->Layers.size());
					// Check whether the stored layer belongs to the loaded Aseprite source.
					if (SelectedSprite->LayerIndex >= 0 && SelectedSprite->LayerIndex < LayerCount)
					{
						TimelineEvent.LayerIndex = SelectedSprite->LayerIndex;
					}
				}
				SendEvent(TimelineEvent);
			}
		}
	}
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
		Draw_FrameInversionPopup();
		Draw_SourceZXDiffPopup();

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
		const bool bConvertToIPM = bSource && bNeedConvertCanvasToZX;
		const bool bConvertToSource = !bSource && bNeedConvertZXToCanvas;
		const bool bConvertEnabled = bConvertToIPM || bConvertToSource;
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
		const char* Symbol = !bConvertEnabled ? "=" : bConvertToIPM ? ">" : "<";
		if (UI::Button(Symbol, bSource, { WidthConvert, WidthConvert }, bConvertEnabled))
		{
			if (bConvertToIPM)
			{
				ConversionToZX(ConversationSettings);
				bIPMDirty = true;
			}
			else if (bConvertToSource)
			{
				ConversionToCanvas(ConversationSettings);
				if (ImageFormat == EImageFormat::Aseprite && UpdateAsepriteFrameFromSource())
				{
					bSourceDirty = false;
					bAsepriteSourceDirty = true;
				}
				else
				{
					bSourceDirty = true;
				}
				NotifySpritesUpdated(
					SelectedSpritesFrame,
					ZXColorView->IndexedData,
					ZXColorView->InkData,
					ZXColorView->AttributeData,
					ZXColorView->MaskData);
			}
			bNeedConvertCanvasToZX = false;
			bNeedConvertZXToCanvas = false;
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
			Event.CanvasName = GetWindowWName();
			Event.OptionsFlags = OptionsFlags[0];
			SendEvent(Event);
		}

		ImGui::BeginChild("Child", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBringToFrontOnFocus);
		CanvasID = ImGui::GetCurrentWindow()->ID;
		// Update only the displayed image after the comparison controls have been processed.
		UpdateSourceZXDiffPreview();
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
	FEvent_AppSprite AppSprite_Event(FEventTag::NotificationRemoveCanvasTag);
	{
		AppSprite_Event.Canvas = shared_from_this();
		SendEvent(AppSprite_Event);
	}

	UI::Draw_ZXColorView_Shutdown(ZXColorView);
	UnsubscribeAll();
}

void SCanvas::Draw_PopupMenu()
{
	const ImGuiID CreateSpriteID = ImGui::GetCurrentWindow()->GetID(CreateSpriteName);
	auto DrawLastItemTooltip = [](const char* Tooltip)
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted(Tooltip);
				ImGui::EndTooltip();
			}
		};

	auto AddRegionLambda = [=, this]() -> bool
		{
			const std::shared_ptr<FSprite>& Sprite = SelectedSprite;
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

			Sprite->Regions.push_back(NewRegion);
			return true;
		};

	if (bOpenPopupMenu = ImGui::BeginPopup(PopupMenuName))
	{
		if (ImGui::MenuItem("Add Sprite"))
		{
			ImGui::OpenPopup(CreateSpriteID);
		}
		if (SelectedSprite && ImGui::MenuItem("Add Region"))
		{
			AddRegionLambda();
		}
		auto ApplyLimitArea = [this](bool bAllFrames)
		{
			const ImRect SelectedArea = ZXColorView->RectangleMarqueeRect;
			FEvent_RequestTimelineState Event_Timeline;
			Event_Timeline.Callback =
				[this, SelectedArea, bAllFrames](const FTimelineState& TimelineState)
				{
					const int32_t FirstFrame = bAllFrames ? 0 : TimelineState.CurrentFrame;
					const int32_t LastFrame = bAllFrames && AsepriteSprite
						? static_cast<int32_t>(AsepriteSprite->Frames.size())
						: FirstFrame + 1;
					const FTilemapCellData_Rect LimitArea(SelectedArea);
					for (int32_t Frame = FirstFrame; Frame < LastFrame; ++Frame)
					{
						FPropertyBag* Property = Keyframes
							? Keyframes->GetMutableProperty(Frame, TimelineState.CurrentLayer)
							: nullptr;
						if (Property != nullptr && !Property->SetStruct(FPropertyTag::LimitArea, LimitArea))
						{
							Property->AddStruct(FPropertyTag::LimitArea, LimitArea);
						}
					}
				};
			SendEvent(Event_Timeline);
		};

		if (AsepriteSprite && AsepriteSprite->IsValid() && ImGui::MenuItem("Limit Area"))
		{
			ApplyLimitArea(false);
		}
		if (AsepriteSprite && AsepriteSprite->IsValid() && ImGui::MenuItem("Limit Area (All Frames)"))
		{
			ApplyLimitArea(true);
		}
		DrawLastItemTooltip("Применить текущее выделение ко всем кадрам текущего слоя.");
		if (AsepriteSprite && AsepriteSprite->IsValid() && ImGui::MenuItem("Инверсия..."))
		{
			FrameInversionRect = ZXColorView->RectangleMarqueeRect;
			FrameInversionError.clear();
			bOpenFrameInversionPopup = true;
		}
		DrawLastItemTooltip("Инвертировать пиксели и/или поменять местами INK и PAPER в выделенной области.");
		ImGui::EndPopup();	
	}
	
	Draw_PopupMenu_CreateSprite();
}

void SCanvas::Draw_FrameInversionPopup()
{
	static const char* PopupName = "Инверсия ZX##FrameInversion";
	if (bOpenFrameInversionPopup)
	{
		ImGui::OpenPopup(PopupName);
		bOpenFrameInversionPopup = false;
	}

	if (!ImGui::BeginPopupModal(PopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	auto DrawLastItemTooltip = [](const char* Tooltip)
	{
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(Tooltip);
			ImGui::EndTooltip();
		}
	};

	ImGui::Checkbox("Пиксели", &bInvertFramePixels);
	DrawLastItemTooltip("Инвертировать bitmap-биты только внутри выделения.");

	ImGui::Checkbox("Атрибуты", &bInvertFrameAttributes);
	DrawLastItemTooltip("Поменять местами цвета INK и PAPER в затронутых знакоместах.");

	ImGui::Checkbox("Все кадры", &bInvertAllFrames);
	DrawLastItemTooltip("Применить операцию ко всем кадрам и сохранить покадровые override-файлы.");

	if (!FrameInversionError.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.25f, 1.0f), "%s", FrameInversionError.c_str());
	}

	ImGui::Separator();
	ImGui::BeginDisabled(!bInvertFramePixels && !bInvertFrameAttributes);
	if (ImGui::Button("Применить") && ApplyFrameInversion())
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Отмена"))
	{
		FrameInversionError.clear();
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void SCanvas::Draw_SourceZXDiffPopup()
{
	const std::string PopupName = std::format("ZX Differences##SourceZXDiff_{}", GetWindowName());
	// Check whether this canvas can show its pending comparison without replacing another popup.
	if (bCheckSourceZXDiff && IsActiveCanvas() && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
	{
		bCheckSourceZXDiff = false;
		// Discard the previous comparison before checking a newly loaded source.
		SourceZXDiff.reset();
		// Create the old and new ZX versions for this comparison.
		auto Diff = std::make_shared<FSourceZXDiff>();
		// Check whether the current source frame can be compared with its saved ZX files.
		if (BuildSourceZXDiff(SelectedSpritesFrame, *Diff))
		{
			const bool bHasSavedData = Diff->Ink.State != FZXDataDiff::EState::Missing || Diff->Attribute.State != FZXDataDiff::EState::Missing || Diff->Mask.State != FZXDataDiff::EState::Missing;
			bool bHasDifferences = Diff->Ink.State != FZXDataDiff::EState::Equal || Diff->Attribute.State != FZXDataDiff::EState::Equal || Diff->Mask.State != FZXDataDiff::EState::Equal;
			auto CanMergeData = [](const FZXDataDiff& Data) -> bool
			{
				// Check that both versions were read and have matching byte positions.
				return Data.State == FZXDataDiff::EState::Equal || Data.State == FZXDataDiff::EState::Different;
			};
			// Check whether all three components can be merged before deciding to show the window.
			if (CanMergeData(Diff->Ink) && CanMergeData(Diff->Attribute) && CanMergeData(Diff->Mask))
			{
				// Calculate the result of applying the default operations to the saved files again.
				Diff->UpdatePreviewData();
				// Check whether applying this result would change any saved component.
				bHasDifferences = Diff->PreviewInk != Diff->Ink.SavedData || Diff->PreviewAttribute != Diff->Attribute.SavedData || Diff->PreviewMask != Diff->Mask.SavedData;
			}
			// Check whether existing ZX files require a choice instead of a first conversion.
			if (bHasSavedData && bHasDifferences)
			{
				SourceZXDiff = std::move(Diff);
				// Show all three ZX components when the comparison window opens.
				OptionsFlags[0] = FCanvasOptionsFlags::Ink | FCanvasOptionsFlags::Attribute | FCanvasOptionsFlags::Mask;
				bRefreshCanvas = true;
				bPlay = false;
			}
		}
	}

	// Check whether this canvas has a comparison dialog to draw.
	if (!SourceZXDiff)
	{
		return;
	}
	// Draw a floating window so the canvas remains available for navigation.
	bool bWindowOpen = true;
	const bool bVisible = ImGui::Begin(PopupName.c_str(), &bWindowOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking);
	// Check whether the comparison window was collapsed or closed.
	if (!bVisible || !bWindowOpen)
	{
		ImGui::End();
		// Check whether the user closed the window rather than collapsed it.
		if (!bWindowOpen)
		{
			// Release the temporary comparison when its window is closed.
			SourceZXDiff.reset();
		}
		return;
	}

	auto DrawTooltip = [](const char* Text) -> void
	{
		// Check whether the pointer is over this item, including disabled controls.
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("%s", Text);
		}
	};

	ImGui::TextUnformatted(SourcePathFile.filename().string().c_str());
	DrawTooltip("Исходный файл, с которым сравниваются сохранённые ZX-данные.");
	// Check whether the source comparison belongs to a numbered animation frame.
	if (SourceZXDiff->Frame != INDEX_NONE)
	{
		ImGui::Text("Frame: %d", SourceZXDiff->Frame);
		DrawTooltip("Кадр исходника, использованный для сравнения.");
	}
	ImGui::TextUnformatted("Saved ZX data differs from the source conversion.");
	DrawTooltip("Сравниваются сохранённые ZX-файлы и новая конвертация исходника.");
	ImGui::Separator();

	static const char* OperationNames[] = { "AND", "OR", "XOR" };
	auto DrawRow = [this, &DrawTooltip](const char* Label, const char* Tooltip, const FZXDataDiff& Diff, FSourceZXDiff::EOperation& Operation) -> void
	{
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Label);
		DrawTooltip(Tooltip);
		ImGui::TableNextColumn();
		switch (Diff.State)
		{
		case FZXDataDiff::EState::Equal:
			ImGui::TextUnformatted("Identical");
			DrawTooltip("Сохранённые данные и новая конвертация совпадают побайтно.");
			break;
		case FZXDataDiff::EState::Different:
			ImGui::Text("%zu of %zu bytes differ", Diff.DifferentBytes, Diff.SourceData.size());
			DrawTooltip("Количество отличающихся байтов и общий размер данных.");
			break;
		case FZXDataDiff::EState::Missing:
			ImGui::TextUnformatted("File missing");
			DrawTooltip("Сохранённый ZX-файл отсутствует, сравнение и выбор операции недоступны.");
			break;
		case FZXDataDiff::EState::SizeMismatch:
			ImGui::Text("Size mismatch: %zu / %zu bytes", Diff.SavedData.size(), Diff.SourceData.size());
			DrawTooltip("Размеры не совпадают, сначала указан размер сохранённого файла, затем новой конвертации. Выбор операции недоступен.");
			break;
		case FZXDataDiff::EState::ReadError:
			ImGui::TextUnformatted("Read error");
			DrawTooltip("Не удалось прочитать сохранённый ZX-файл, сравнение и выбор операции недоступны.");
			break;
		}
		ImGui::TableNextColumn();
		ImGui::PushID(Label);
		ImGui::SetNextItemWidth(90.0f);
		ImGui::BeginDisabled(Diff.State != FZXDataDiff::EState::Equal && Diff.State != FZXDataDiff::EState::Different);
		int32_t OperationIndex = static_cast<int32_t>(Operation);
		// Check whether the user selected another operation for this ZX component.
		if (ImGui::Combo("##Operation", &OperationIndex, OperationNames, IM_ARRAYSIZE(OperationNames)))
		{
			Operation = static_cast<FSourceZXDiff::EOperation>(OperationIndex);
			SourceZXDiff->bPreviewDirty = true;
		}
		ImGui::EndDisabled();
		DrawTooltip("Операция применяется к соответствующим битам сохранённых данных и новой конвертации.\nAND: бит 1, только если он 1 в обеих версиях.\nOR: бит 1, если он 1 хотя бы в одной версии.\nXOR: бит 1, если значения бита различаются.");
		ImGui::PopID();
	};

	// Create the three component rows with their comparison results and operations.
	if (ImGui::BeginTable("##SourceZXDiff", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("Type");
		ImGui::TableSetupColumn("Differences");
		ImGui::TableSetupColumn("Operation");
		ImGui::TableHeadersRow();
		DrawRow("Ink", "Пиксельные данные из файла .ink.", SourceZXDiff->Ink, SourceZXDiff->InkOperation);
		DrawRow("Paper (.attr)", "Байты атрибутов из файла .attr целиком, включая цвета INK и PAPER, BRIGHT и FLASH.", SourceZXDiff->Attribute, SourceZXDiff->AttributeOperation);
		DrawRow("Mask", "Маска прозрачности из файла .mask.", SourceZXDiff->Mask, SourceZXDiff->MaskOperation);
		ImGui::EndTable();
	}

	ImGui::Separator();
	const bool bCanPreview = CanPreviewSourceZXDiff();
	ImGui::BeginDisabled(!bCanPreview);
	ImGui::Checkbox("Preview", &SourceZXDiff->bPreview);
	ImGui::EndDisabled();
	DrawTooltip(bCanPreview ? "Показывать результат выбранных операций на канвасе без изменения рабочих данных и файлов." : "Предпросмотр доступен при совпадении кадра, размеров и наличии всех ZX-данных.");
	// Check whether this comparison can be displayed on the current canvas frame.
	if (!bCanPreview)
	{
		ImGui::TextDisabled("Preview unavailable.");
		DrawTooltip("Предпросмотр доступен при совпадении кадра, размеров и наличии всех ZX-данных.");
	}
	ImGui::Separator();
	ImGui::BeginDisabled(!bCanPreview);
	// Check whether the user wants to commit the selected operations to the canvas.
	if (ImGui::Button("Apply"))
	{
		ApplySourceZXDiff();
	}
	ImGui::EndDisabled();
	DrawTooltip(bCanPreview ? "Применить выбранные операции к ZX-данным канваса одним действием Undo, без записи файлов." : "Применение доступно при совпадении кадра, размеров и наличии всех ZX-данных.");
	ImGui::SameLine();
	// Check whether the user wants to close the comparison without applying it.
	if (ImGui::Button("Cancel"))
	{
		// Release the temporary comparison when its dialog is closed.
		SourceZXDiff.reset();
	}
	DrawTooltip("Закрыть окно и вернуть обычное отображение канваса без применения результата.");
	ImGui::End();
}

bool SCanvas::CanPreviewSourceZXDiff() const
{
	// Check whether the comparison still belongs to the loaded canvas and selected frame.
	if (!SourceZXDiff || bCheckSourceZXDiff || bFroceRebuiltSpriteFrame || !ZXColorView->Image.IsValid() ||
		Width <= 0 || Height <= 0 || Width % 8 != 0 || Height % 8 != 0 ||
		SourceZXDiff->Width != Width || SourceZXDiff->Height != Height ||
		(ImageFormat == EImageFormat::Aseprite && (SourceZXDiff->Frame != SelectedSpritesFrame || LastRebuiltSpriteFrame != SelectedSpritesFrame)))
	{
		return false;
	}

	// Calculate the byte counts required to display the complete canvas.
	const size_t PixelSize = static_cast<size_t>(Width >> 3) * Height;
	const size_t AttributeSize = static_cast<size_t>(Width >> 3) * (Height >> 3);
	auto HasValidData = [](const FZXDataDiff& Diff, size_t ExpectedSize) -> bool
	{
		// Check that both versions were read completely and have matching byte positions.
		return (Diff.State == FZXDataDiff::EState::Equal || Diff.State == FZXDataDiff::EState::Different) && Diff.SavedData.size() == ExpectedSize && Diff.SourceData.size() == ExpectedSize;
	};
	return HasValidData(SourceZXDiff->Ink, PixelSize) && HasValidData(SourceZXDiff->Attribute, AttributeSize) && HasValidData(SourceZXDiff->Mask, PixelSize);
}

void SCanvas::UpdateSourceZXDiffPreview()
{
	// Check whether preview is enabled and its data can be displayed on this frame.
	if (!CanPreviewSourceZXDiff() || !SourceZXDiff->bPreview)
	{
		// Check whether the texture still contains a preview that must be removed.
		if (bSourceZXPreviewActive)
		{
			// Restore the ordinary display from the unchanged canvas data.
			RebuildCanvasFromAseprite(SelectedSpritesFrame);
			bSourceZXPreviewActive = false;
		}
		return;
	}

	// Build the same temporary result that Apply will use.
	SourceZXDiff->UpdatePreviewData();

	// Select the visible components, showing the complete merged result in source view.
	const bool bSource = OptionsFlags[0] & FCanvasOptionsFlags::Source;
	const bool bInk = bSource || bTransparentMask || (OptionsFlags[0] & FCanvasOptionsFlags::Ink);
	const bool bPaper = bSource || bTransparentMask || (OptionsFlags[0] & FCanvasOptionsFlags::Attribute);
	const bool bMask = bSource || (OptionsFlags[0] & FCanvasOptionsFlags::Mask);
	const bool bTransparentPaper = AsepriteSprite && !AsepriteSprite->InkLayer.empty();
	// Convert only the display texture, leaving working arrays, dirty flags and Undo unchanged.
	UI::ZXAttributeColorToImage(ZXColorView->Image, Width, Height, bInk ? SourceZXDiff->PreviewInk.data() : nullptr, bPaper ? SourceZXDiff->PreviewAttribute.data() : nullptr, bMask ? SourceZXDiff->PreviewMask.data() : nullptr, false, nullptr, true, bTransparentMask, bTransparentPaper);
	bSourceZXPreviewActive = true;
}

void SCanvas::ApplySourceZXDiff()
{
	// Check that both versions still describe the complete current canvas.
	if (!CanPreviewSourceZXDiff())
	{
		return;
	}
	// Build the result even when Preview is switched off.
	SourceZXDiff->UpdatePreviewData();
	// Check whether applying the result would actually change any ZX data.
	if (ZXColorView->InkData == SourceZXDiff->PreviewInk && ZXColorView->AttributeData == SourceZXDiff->PreviewAttribute && ZXColorView->MaskData == SourceZXDiff->PreviewMask)
	{
		// Check whether the unchanged canvas result still differs from the saved ZX files.
		bIPMDirty |= ZXColorView->InkData != SourceZXDiff->Ink.SavedData || ZXColorView->AttributeData != SourceZXDiff->Attribute.SavedData || ZXColorView->MaskData != SourceZXDiff->Mask.SavedData;
		// Check whether the unchanged ZX result is currently hidden by the source view.
		if (OptionsFlags[0] & FCanvasOptionsFlags::Source)
		{
			OptionsFlags[0] = FCanvasOptionsFlags::Ink | FCanvasOptionsFlags::Attribute | FCanvasOptionsFlags::Mask;
		}
		SourceZXDiff.reset();
		RebuildCanvasFromAseprite(SelectedSpritesFrame);
		bRefreshCanvas = false;
		bSourceZXPreviewActive = false;
		return;
	}

	// Create one action that is also its own Undo and Redo boundary.
	auto Action = std::make_shared<FSourceZXApplyAction>();
	Action->InkData = SourceZXDiff->PreviewInk;
	Action->AttributeData = SourceZXDiff->PreviewAttribute;
	Action->MaskData = SourceZXDiff->PreviewMask;
	Action->Swap = [this, Frame = SelectedSpritesFrame, CanvasWidth = Width, CanvasHeight = Height, Generation = ZXDataGeneration](FSourceZXApplyAction& Param) -> void
	{
		// Check that this snapshot still belongs to the current frame and loaded ZX data.
		if (SelectedSpritesFrame != Frame || Width != CanvasWidth || Height != CanvasHeight || ZXDataGeneration != Generation || bFroceRebuiltSpriteFrame)
		{
			LOG_WARNING("[ApplySourceZXDiff] Undo snapshot no longer matches the loaded canvas.");
			return;
		}
		// Exchange the complete ZX result with the state stored for the next Undo or Redo.
		ZXColorView->InkData.swap(Param.InkData);
		ZXColorView->AttributeData.swap(Param.AttributeData);
		ZXColorView->MaskData.swap(Param.MaskData);
		std::swap(bNeedConvertZXToCanvas, Param.bNeedConvertZXToCanvas);
		// Check whether the changed ZX data needs to be shown instead of the source.
		if (OptionsFlags[0] & FCanvasOptionsFlags::Source)
		{
			OptionsFlags[0] = FCanvasOptionsFlags::Ink | FCanvasOptionsFlags::Attribute | FCanvasOptionsFlags::Mask;
		}
		// Mark restored data as unsaved even when Save was used between Apply and Undo.
		bIPMDirty = true;
		bRefreshCanvas = true;
	};
	// Check whether an earlier pencil stroke needs its own boundary closed first.
	if (UndoQueue.IsContinuous())
	{
		UndoQueue.EndContinuous();
	}
	UndoQueue.SetWithUndo(Action);
	// Close the comparison and display the applied working data immediately.
	SourceZXDiff.reset();
	RebuildCanvasFromAseprite(SelectedSpritesFrame);
	bRefreshCanvas = false;
	bSourceZXPreviewActive = false;
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
		std::vector<FSpriteNameOption> SpriteNameOptions;
		if (ImGui::IsWindowAppearing())
		{
			CreateSpriteSize = ZXColorView->RectangleMarqueeRect.GetSize();

			SpriteNames.clear();
			FEvent_Sprite RequestNames(FEventTag::RequestAllSpritesTag);
			SendEvent(RequestNames);
			SpriteNameOptions = GetSpriteNameOptions();
			auto LastSelected = std::find_if(SpriteNameOptions.begin(), SpriteNameOptions.end(),
				[](const FSpriteNameOption& Option) { return Option.Base == LastSelectedSpriteNameBase; });
			SelectedSpriteNameBase = LastSelected != SpriteNameOptions.end()
				? LastSelected->Base
				: (SpriteNameOptions.empty() ? std::string{} : SpriteNameOptions.front().Base);
			const std::string NextSpriteName = GetNextSpriteName(SpriteNameOptions, SelectedSpriteNameBase);
			snprintf(CreateSpriteNameBuffer, IM_ARRAYSIZE(CreateSpriteNameBuffer), "%s", NextSpriteName.c_str());
			sprintf(CreateSpriteWidthBuffer, "%i\n", int(CreateSpriteSize.x));
			sprintf(CreateSpriteHeightBuffer, "%i\n", int(CreateSpriteSize.y));

			bRoundingToMultipleEight = true;
			bRectangularSprite = false;

			const ImVec2 OriginalSpriteSize = ZXColorView->RectangleMarqueeRect.GetSize();
			Log2SpriteSize = { powf(2.0f, ceilf(log2f(OriginalSpriteSize.x))), powf(2.0f, ceilf(log2f(OriginalSpriteSize.y))) };

			bUpdateSize = true;
		}
		else
		{
			SpriteNameOptions = GetSpriteNameOptions();
		}

		const float TextWidth = ImGui::CalcTextSize("A").x;
		const float TextHeight = ImGui::GetTextLineHeightWithSpacing();

		const ImGuiInputTextFlags InputNumberTextFlags = ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackEdit;

		ImGui::Dummy(ImVec2(0.0f, TextHeight * 0.5f));
		ImGui::Text("Name : ");
		ImGui::SameLine(50.0f);
		if (SpriteNameOptions.size() > 1)
		{
			ImGui::SetNextItemWidth(TextWidth * 18.0f);
			const std::string PreviewBase = SelectedSpriteNameBase.empty() ? "Select name" : SelectedSpriteNameBase;
			if (ImGui::BeginCombo("##SpriteNameBase", PreviewBase.c_str()))
			{
				for (const FSpriteNameOption& Option : SpriteNameOptions)
				{
					const bool bSelected = Option.Base == SelectedSpriteNameBase;
					const std::string OptionLabel = std::format("{} ({})##{}", Option.Base, Option.Count, Option.Base);
					if (ImGui::Selectable(OptionLabel.c_str(), bSelected))
					{
						SelectedSpriteNameBase = Option.Base;
						LastSelectedSpriteNameBase = Option.Base;
						const std::string NextSpriteName = GetNextSpriteName(SpriteNameOptions, Option.Base);
						snprintf(CreateSpriteNameBuffer, IM_ARRAYSIZE(CreateSpriteNameBuffer), "%s", NextSpriteName.c_str());
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
		}
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
			std::string SelectedBase;
			int32_t SelectedNumber = 0;
			if (SplitSpriteName(CreateSpriteNameBuffer, SelectedBase, SelectedNumber))
			{
				SelectedSpriteNameBase = SelectedBase;
				LastSelectedSpriteNameBase = SelectedBase;
			}
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
			Event.SourcePathFile = SourcePathFile.empty() ? std::filesystem::path(GetWindowWName()) : SourcePathFile;

			Event.IndexedData = ZXColorView->IndexedData;
			Event.InkData = ZXColorView->InkData;
			Event.AttributeData = ZXColorView->AttributeData;
			Event.MaskData = ZXColorView->MaskData;
			Event.AsepriteIndex = ImageFormat == EImageFormat::Aseprite ? SelectedSpritesFrame : ImageFrameIndex;
			if (ImageFormat == EImageFormat::Aseprite && AsepriteSprite)
			{
				// Request the active editor layer before creating the sprite.
				FEvent_RequestTimelineState RequestLayerState;
				RequestLayerState.Callback =
					[this, &Event](const FTimelineState& EditorState)
					{
						// Calculate the number of source layers available for the new sprite.
						const int32_t LayerCount = static_cast<int32_t>(AsepriteSprite->Layers.size());
						// Check that the active editor layer belongs to the loaded Aseprite source.
						if (EditorState.CurrentLayer < 0 || EditorState.CurrentLayer >= LayerCount)
						{
							return;
						}

						// Calculate the internal Aseprite layer index stored from bottom to top.
						Event.LayerIndex = LayerCount - EditorState.CurrentLayer - 1;
					};
				SendEvent(RequestLayerState);

				Event.InkLayer = AsepriteSprite->InkLayer;
				Event.AttributeLayer = AsepriteSprite->AttributeLayer;
				Event.MaskLayer = AsepriteSprite->MaskLayer;
			}
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
	if (ActiveCanvas.lock().get() != this)
	{
		return;
	}

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

	//// hard reset any popup menu
	//ImGui::CloseCurrentPopup();

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

		FEvent_Canvas Event;
		Event.Tag = FEventTag::CanvasViewScaleTag;
		Event.CanvasName = GetWindowWName();
		Event.CanvasWidth = Width;
		Event.CanvasHeight = Height;
		Event.MouseWheel = MouseWheel;
		SendEvent(Event);
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

			FEvent_Canvas Event;
			Event.Tag = FEventTag::CanvasViewPositionTag;
			Event.CanvasName = GetWindowWName();
			Event.CanvasWidth = Width;
			Event.CanvasHeight = Height;
			Event.ImagePosition = Delta;
			SendEvent(Event);
		}
	}
}

void SCanvas::SetToolMode(EToolMode::Type NewToolMode, bool bForce /*= true*/, bool bEvent /*= false*/)
{
	const EToolMode::Type PreviousToolMode = ToolMode[0];

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

	if (PreviousToolMode != ToolMode[0] && ToolMode[0] == EToolMode::None)
	{
		Reset_RectangleMarquee();
	}

	if (!bEvent)
	{
		FEvent_Canvas Event;
		Event.Tag = FEventTag::ChangeToolModeTag;
		Event.CanvasName = GetWindowWName();
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

void SCanvas::Imput_SelectAll()
{
	if (ToolMode[0] != EToolMode::RectangleMarquee)
	{
		return;
	}

	ZXColorView->bVisibilityRectangleMarquee = true;
	bRectangleMarqueeActive = true;

	ImVec2 p1 = ImVec2();
	ImVec2 p2 = ImVec2(ZXColorView->Image.Size.x, ZXColorView->Image.Size.y);

	// normalize the rectangle (Min is always to the left/above, Max is to the right/below)
	ZXColorView->RectangleMarqueeRect.Min = ImVec2(ImMin(p1.x, p2.x), ImMin(p1.y, p2.y));
	ZXColorView->RectangleMarqueeRect.Max = ImVec2(ImMax(p1.x, p2.x), ImMax(p1.y, p2.y));

	// last pixel inclusion compensation
	ZXColorView->RectangleMarqueeRect.Max.x += 1.0f;
	ZXColorView->RectangleMarqueeRect.Max.y += 1.0f;

	ZXColorView->RectangleMarqueeRect.Min = ImClamp(ZXColorView->RectangleMarqueeRect.Min, ImVec2(0, 0), ZXColorView->Image.Size);
	ZXColorView->RectangleMarqueeRect.Max = ImClamp(ZXColorView->RectangleMarqueeRect.Max, ImVec2(0, 0), ZXColorView->Image.Size);
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
			bSourceDirty = true;
			bNeedConvertCanvasToZX = true;
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
			bSourceDirty = true;
			UI::FillRegion(FullRect, ZXColorView->IndexedData, Width, Height, EZXColor::Transparent);
			bNeedConvertCanvasToZX = true;
		}
		else
		{
			bIPMDirty = true;
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
	// Save pending ZX changes regardless of whether the source is currently displayed.
	const bool bSource = !bIPMDirty && (OptionsFlags[0] & FCanvasOptionsFlags::Source);
	const bool bAsepriteSourcePending = bSource && ImageFormat == EImageFormat::Aseprite && bAsepriteSourceDirty;
	if ((bSource && !bSourceDirty && !bAsepriteSourcePending) ||
		(!bSource && !bIPMDirty))
	{
		return;
	}

	if (bSource && ImageFormat == EImageFormat::Aseprite)
	{
		if (bSourceDirty && UpdateAsepriteFrameFromSource())
		{
			bSourceDirty = false;
			bAsepriteSourceDirty = true;
			NotifySpritesUpdated(
				SelectedSpritesFrame,
				ZXColorView->IndexedData,
				ZXColorView->InkData,
				ZXColorView->AttributeData,
				ZXColorView->MaskData);
		}
		LOG_WARNING("[{}]\t Saving the Aseprite source file is not implemented yet.", (__FUNCTION__)); // ToDo: save *.aseprite
		return;
	}

	if (SourcePathFile.empty())
	{
		return;
	}

	const std::filesystem::path SavePath = SourcePathFile.parent_path();
	const std::filesystem::path SaveName = SourcePathFile.stem();
	const bool bSaved = bSource
		? SaveSource(SavePath, SaveName)
		: SaveIPM(SavePath, SaveName);
	if (!bSaved)
	{
		return;
	}

	if (bSource)
	{
		bSourceDirty = false;
	}
	else
	{
		bIPMDirty = false;
	}

	NotifySpritesUpdated(
		ImageFormat == EImageFormat::Aseprite ? SelectedSpritesFrame : ImageFrameIndex,
		ZXColorView->IndexedData,
		ZXColorView->InkData,
		ZXColorView->AttributeData,
		ZXColorView->MaskData);
}

bool SCanvas::PrepareToChangeFrame()
{
	if (ImageFormat == EImageFormat::Aseprite && bSourceDirty)
	{
		if (!UpdateAsepriteFrameFromSource())
		{
			return false;
		}
		bSourceDirty = false;
		bAsepriteSourceDirty = true;
		NotifySpritesUpdated(
			SelectedSpritesFrame,
			ZXColorView->IndexedData,
			ZXColorView->InkData,
			ZXColorView->AttributeData,
			ZXColorView->MaskData);
	}

	const bool bSource = OptionsFlags[0] & FCanvasOptionsFlags::Source;
	if ((bSource && bSourceDirty) || (!bSource && bIPMDirty))
	{
		Imput_Save();
	}

	if (bIPMDirty || bSourceDirty)
	{
		return false;
	}
	return true;
}

void SCanvas::Imput_PreviousFrame()
{
	switch (ImageFormat)
	{
	case EImageFormat::None:
	case EImageFormat::Create:
	case EImageFormat::JSON:
	case EImageFormat::PNG:
	default:
		return;
	case EImageFormat::GIF:
	case EImageFormat::Aseprite:
		break;
	}

	if (!PrepareToChangeFrame())
	{
		return;
	}

	if (SelectedSpritesFrame == 0)
	{
		SelectedSpritesFrame = MaxFramesInSprites;
	}
	else
	{
		SelectedSpritesFrame--;
	}

	PlayDuration = ImageFormat == EImageFormat::Aseprite ? float(AsepriteSprite->DurationPerFrame[SelectedSpritesFrame]) * 0.001f : 0.05f;

	FEvent_Timeline Event(FEventTag::TimelineChangedFrameTag);
	{
		Event.Frame = SelectedSpritesFrame;
		Event.Format = ImageFormat;
		SendEvent(Event);
	}

	switch (ImageFormat)
	{
	case EImageFormat::GIF:
		break;
	case EImageFormat::Aseprite:
		bRefreshCanvas = true;
		break;
	}
}

void SCanvas::Imput_Play()
{
	if (ActiveCanvas.lock().get() != this)
	{
		return;
	}

	switch (ImageFormat)
	{
	case EImageFormat::None:
	case EImageFormat::Create:
	case EImageFormat::JSON:
	case EImageFormat::PNG:
	default:
		bPlay = false;
		return;
	case EImageFormat::GIF:
	case EImageFormat::Aseprite:
		break;
	}

	if (ImageFormat == EImageFormat::Aseprite)
	{
		if (!AsepriteSprite || AsepriteSprite->DurationPerFrame.empty())
		{
			bPlay = false;
			PlayDuration = 0.0f;
			return;
		}

		if (SelectedSpritesFrame < 0 ||
			SelectedSpritesFrame >= static_cast<int32_t>(AsepriteSprite->DurationPerFrame.size()))
		{
			SelectedSpritesFrame = 0;
			bRefreshCanvas = true;
		}
	}

	bPlay = !bPlay;
	PlayDuration = ImageFormat == EImageFormat::Aseprite ? float(AsepriteSprite->DurationPerFrame[SelectedSpritesFrame]) * 0.001f : 0.05f;
}

void SCanvas::Imput_NextFrame()
{
	switch (ImageFormat)
	{
	case EImageFormat::None:
	case EImageFormat::Create:
	case EImageFormat::JSON:
	case EImageFormat::PNG:
	default:
		return;
	case EImageFormat::GIF:
	case EImageFormat::Aseprite:
		break;
	}

	if (!PrepareToChangeFrame())
	{
		return;
	}

	if (SelectedSpritesFrame >= MaxFramesInSprites)
	{
		SelectedSpritesFrame = 0;
	}
	else
	{
		SelectedSpritesFrame++;
	}
	PlayDuration = ImageFormat == EImageFormat::Aseprite ? float(AsepriteSprite->DurationPerFrame[SelectedSpritesFrame]) * 0.001f : 0.05f;

	FEvent_Timeline Event(FEventTag::TimelineChangedFrameTag);
	{
		Event.Frame = SelectedSpritesFrame;
		Event.Format = ImageFormat;
		SendEvent(Event);
	}

	switch (ImageFormat)
	{
	case EImageFormat::GIF:
		break;
	case EImageFormat::Aseprite:
		bRefreshCanvas = true;
		break;
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
	auto UpdateRectangleMarquee = [this]()
		{
			const ImVec2& p1 = ZXColorView->RectStart;
			const ImVec2& p2 = ZXColorView->RectEnd;
			ZXColorView->RectangleMarqueeRect.Min = ImVec2(ImMin(p1.x, p2.x), ImMin(p1.y, p2.y));
			ZXColorView->RectangleMarqueeRect.Max = ImVec2(ImMax(p1.x, p2.x) + 1.0f, ImMax(p1.y, p2.y) + 1.0f);
			ZXColorView->RectangleMarqueeRect.Min = ImClamp(ZXColorView->RectangleMarqueeRect.Min, ImVec2(0, 0), ZXColorView->Image.Size);
			ZXColorView->RectangleMarqueeRect.Max = ImClamp(ZXColorView->RectangleMarqueeRect.Max, ImVec2(0, 0), ZXColorView->Image.Size);
			ZXColorView->bVisibilityRectangleMarquee =
				ZXColorView->RectangleMarqueeRect.GetWidth() > 0.0f &&
				ZXColorView->RectangleMarqueeRect.GetHeight() > 0.0f;
		};

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

	if (IO.MouseClicked[ImGuiMouseButton_Left])
	{
		ZXColorView->RectStart = UI::ConverZXViewPositionToPixel(*ZXColorView, ImGui::GetMousePos());
		ZXColorView->RectEnd = ZXColorView->RectStart;
		ZXColorView->RectangleMarqueeRect = ImRect(0.0f, 0.0f, 0.0f, 0.0f);
		ZXColorView->bVisibilityRectangleMarquee = false;
		bRectangleMarqueeActive = true;
	}
	else if (!bOpenPopupMenu && bRectangleMarqueeActive && IO.MouseDown[ImGuiMouseButton_Left])
	{
		if (ZXColorView->bVisibilityRectangleMarquee || ImGui::IsMouseDragging(ImGuiMouseButton_Left))
		{
			ZXColorView->RectEnd = UI::ConverZXViewPositionToPixel(*ZXColorView, ImGui::GetMousePos());
			UpdateRectangleMarquee();
		}
	}
	else if (IO.MouseReleased[ImGuiMouseButton_Left])
	{
		bRectangleMarqueeActive = false;
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

std::filesystem::path SCanvas::GetAsepriteFrameOverridePath(int32_t Frame, const char* Extension) const
{
	if (SourcePathFile.empty() || Frame < 0)
	{
		return {};
	}

	const std::string Filename = std::format(
		"{}_frame_{}{}",
		SourcePathFile.stem().string(),
		Frame,
		Extension);
	return IO::NormalizePath(std::filesystem::absolute(SourcePathFile.parent_path() / Filename));
}

bool SCanvas::LoadAsepriteFrameOverride(
	int32_t Frame,
	std::vector<uint8_t>& InkData,
	std::vector<uint8_t>& AttributeData,
	std::vector<uint8_t>& MaskData) const
{
	if (ImageFormat != EImageFormat::Aseprite || SourcePathFile.empty())
	{
		return false;
	}

	const size_t ExpectedPixelSize = static_cast<size_t>(Width >> 3) * Height;
	const size_t ExpectedAttributeSize = static_cast<size_t>(Width >> 3) * (Height >> 3);
	bool bLoaded = false;

	auto LoadData = [&bLoaded](const std::filesystem::path& Path, size_t ExpectedSize, std::vector<uint8_t>& Output)
	{
		if (Path.empty() || !std::filesystem::exists(Path))
		{
			return;
		}

		std::vector<uint8_t> Data;
		const std::error_code Error = IO::LoadBinaryData(Data, Path);
		if (Error || Data.size() != ExpectedSize)
		{
			LOG_ERROR("[LoadAsepriteFrameOverride] Invalid override file: {}", Path.string());
			return;
		}

		Output = std::move(Data);
		bLoaded = true;
	};

	LoadData(GetAsepriteFrameOverridePath(Frame, ".ink"), ExpectedPixelSize, InkData);
	LoadData(GetAsepriteFrameOverridePath(Frame, ".attr"), ExpectedAttributeSize, AttributeData);
	LoadData(GetAsepriteFrameOverridePath(Frame, ".mask"), ExpectedPixelSize, MaskData);
	return bLoaded;
}

bool SCanvas::SaveAsepriteFrameOverride(
	int32_t Frame,
	const std::vector<uint8_t>& InkData,
	const std::vector<uint8_t>& AttributeData,
	const std::vector<uint8_t>& MaskData) const
{
	const size_t ExpectedPixelSize = static_cast<size_t>(Width >> 3) * Height;
	const size_t ExpectedAttributeSize = static_cast<size_t>(Width >> 3) * (Height >> 3);
	if (ImageFormat != EImageFormat::Aseprite ||
		SourcePathFile.empty() ||
		Frame < 0 ||
		InkData.size() != ExpectedPixelSize ||
		AttributeData.size() != ExpectedAttributeSize ||
		MaskData.size() != ExpectedPixelSize)
	{
		return false;
	}

	const std::error_code InkError = IO::SaveBinaryData(InkData, GetAsepriteFrameOverridePath(Frame, ".ink"), false);
	const std::error_code AttributeError = IO::SaveBinaryData(AttributeData, GetAsepriteFrameOverridePath(Frame, ".attr"), false);
	const std::error_code MaskError = IO::SaveBinaryData(MaskData, GetAsepriteFrameOverridePath(Frame, ".mask"), false);
	if (InkError || AttributeError || MaskError)
	{
		LOG_ERROR("[SaveAsepriteFrameOverride] Failed to save frame {}", Frame);
		return false;
	}
	return true;
}

bool SCanvas::ApplyAsepriteLayerOverrides(
	int32_t Frame,
	std::vector<uint8_t>& InkData,
	std::vector<uint8_t>& AttributeData,
	std::vector<uint8_t>& MaskData) const
{
	if (!AsepriteSprite || ImageFormat != EImageFormat::Aseprite)
	{
		return false;
	}

	auto ConvertAttributeLayer = [this](
		const std::vector<uint8_t>& RGBA,
		std::vector<uint8_t>& OutputAttributeData)
		{
			std::vector<uint8_t> IndexedData;
			std::vector<uint8_t> IgnoredInkData;
			std::vector<uint8_t> IgnoredMaskData;
			UI::QuantizeToZX(
				RGBA.data(),
				Width,
				Height,
				4,
				IndexedData,
				TransparentColor);
			UI::ZXIndexColorToZXAttributeColor(
				IndexedData,
				Width,
				Height,
				IgnoredInkData,
				OutputAttributeData,
				IgnoredMaskData,
				ConversationSettings);
		};

	bool bApplied = false;
	std::vector<uint8_t> LayerRGBA;
	if (AsepriteFormat::GetLayerFrameRGBA(*AsepriteSprite, Frame, AsepriteSprite->InkLayer, LayerRGBA))
	{
		UI::ZXAlphaToPixelData(LayerRGBA.data(), Width, Height, 4, InkData);
		bApplied = true;
	}
	if (AsepriteFormat::GetLayerFrameRGBA(*AsepriteSprite, Frame, AsepriteSprite->MaskLayer, LayerRGBA))
	{
		UI::ZXAlphaToPixelData(LayerRGBA.data(), Width, Height, 4, MaskData, true);
		bApplied = true;
	}
	if (AsepriteFormat::GetLayerFrameRGBA(*AsepriteSprite, Frame, AsepriteSprite->AttributeLayer, LayerRGBA))
	{
		ConvertAttributeLayer(LayerRGBA, AttributeData);
		bApplied = true;
	}
	return bApplied;
}

bool SCanvas::BuildAsepriteFrameZXData(
	int32_t Frame,
	std::vector<uint8_t>& InkData,
	std::vector<uint8_t>& AttributeData,
	std::vector<uint8_t>& MaskData,
	bool bLoadFrameOverride /*= true*/) const
{
	if (!AsepriteSprite ||
		!AsepriteSprite->IsValid() ||
		Frame < 0 ||
		Frame >= static_cast<int32_t>(AsepriteSprite->Frames.size()))
	{
		return false;
	}

	std::vector<uint8_t> IndexedData;
	UI::QuantizeToZX(AsepriteSprite->Frames[Frame].data(), Width, Height, 4, IndexedData, TransparentColor);
	UI::ZXIndexColorToZXAttributeColor(IndexedData, Width, Height, InkData, AttributeData, MaskData, ConversationSettings);
	// Check whether the caller needs saved edits in addition to the source data.
	if (bLoadFrameOverride)
	{
		// Load the saved frame data before applying the assigned source layers.
		LoadAsepriteFrameOverride(Frame, InkData, AttributeData, MaskData);
	}
	ApplyAsepriteLayerOverrides(Frame, InkData, AttributeData, MaskData);
	return true;
}

bool SCanvas::BuildPNGSourceZXData(
	int32_t& SourceWidth,
	int32_t& SourceHeight,
	std::vector<uint8_t>& IndexedData,
	std::vector<uint8_t>& InkData,
	std::vector<uint8_t>& AttributeData,
	std::vector<uint8_t>& MaskData) const
{
	// Check whether this canvas has a PNG source to convert.
	if (ImageFormat != EImageFormat::PNG || SourcePathFile.empty())
	{
		return false;
	}

	// Load the source image independently of the canvas and saved ZX data.
	uint8_t* ImageData = FImageBase::LoadToMemory(SourcePathFile, SourceWidth, SourceHeight);
	// Check whether the source image was loaded with valid dimensions.
	if (!ImageData || SourceWidth <= 0 || SourceHeight <= 0)
	{
		FImageBase::ReleaseLoadedIntoMemory(ImageData);
		return false;
	}

	// Convert the source pixels using the current transparency and ZX settings.
	UI::QuantizeToZX(ImageData, SourceWidth, SourceHeight, 4, IndexedData, TransparentColor);
	FImageBase::ReleaseLoadedIntoMemory(ImageData);
	UI::ZXIndexColorToZXAttributeColor(IndexedData, SourceWidth, SourceHeight, InkData, AttributeData, MaskData, ConversationSettings);
	return true;
}

bool SCanvas::BuildSourceZXDiff(int32_t Frame, FSourceZXDiff& Output) const
{
	// Check whether this canvas has a supported source file to compare.
	if (!CanReloadFromSource())
	{
		return false;
	}

	FSourceZXDiff Result;
	// Check which source format supplies the temporary ZX data.
	if (ImageFormat == EImageFormat::Aseprite)
	{
		// Convert the loaded source frame with its assigned layers and no saved ZX data.
		if (!BuildAsepriteFrameZXData(Frame, Result.Ink.SourceData, Result.Attribute.SourceData, Result.Mask.SourceData, false))
		{
			return false;
		}
		Result.Width = Width;
		Result.Height = Height;
		Result.Frame = Frame;
	}
	else
	{
		std::vector<uint8_t> IndexedData;
		// Convert the PNG source independently of the current canvas data.
		if (!BuildPNGSourceZXData(Result.Width, Result.Height, IndexedData, Result.Ink.SourceData, Result.Attribute.SourceData, Result.Mask.SourceData))
		{
			return false;
		}
	}

	auto CompareData = [this, Frame](const char* Extension, FZXDataDiff& Diff) -> void
	{
		std::filesystem::path DataPath;
		// Check whether the saved ZX filename must include an Aseprite frame index.
		if (ImageFormat == EImageFormat::Aseprite)
		{
			DataPath = GetAsepriteFrameOverridePath(Frame, Extension);
		}
		else
		{
			// Replace the PNG extension with the corresponding ZX data extension.
			DataPath = SourcePathFile;
			DataPath.replace_extension(Extension);
			DataPath = IO::NormalizePath(std::filesystem::absolute(DataPath));
		}

		// Read the saved file size to distinguish missing, empty and inaccessible files.
		const uintmax_t FileSize = std::filesystem::file_size(DataPath, Diff.Error);
		// Check whether the file size could be read before loading its contents.
		if (Diff.Error)
		{
			Diff.State = Diff.Error == std::errc::no_such_file_or_directory ? FZXDataDiff::EState::Missing : FZXDataDiff::EState::ReadError;
			return;
		}

		// Check whether the saved file has any bytes to load.
		if (FileSize > 0)
		{
			// Load the saved bytes without replacing the source conversion.
			Diff.Error = IO::LoadBinaryData(Diff.SavedData, DataPath);
			// Check whether all saved bytes were read successfully.
			if (Diff.Error)
			{
				// Discard partial data so it cannot be used as a complete saved version.
				Diff.SavedData.clear();
				Diff.State = Diff.Error == std::errc::no_such_file_or_directory ? FZXDataDiff::EState::Missing : FZXDataDiff::EState::ReadError;
				return;
			}
		}

		// Check whether the two versions have matching byte positions to compare.
		if (Diff.SavedData.size() != Diff.SourceData.size())
		{
			Diff.State = FZXDataDiff::EState::SizeMismatch;
			return;
		}

		// Calculate how many bytes differ between the saved and converted versions.
		for (size_t Index = 0; Index < Diff.SourceData.size(); ++Index)
		{
			Diff.DifferentBytes += Diff.SavedData[Index] != Diff.SourceData[Index];
		}
		Diff.State = Diff.DifferentBytes > 0 ? FZXDataDiff::EState::Different : FZXDataDiff::EState::Equal;
	};

	// Compare each saved ZX component with its own source conversion.
	CompareData(".ink", Result.Ink);
	CompareData(".attr", Result.Attribute);
	CompareData(".mask", Result.Mask);
	Output = std::move(Result);
	return true;
}

bool SCanvas::SaveSource(const std::filesystem::path& SavePath, const std::filesystem::path& SaveName)
{
	if (ImageFormat == EImageFormat::Aseprite)
	{
		UpdateAsepriteFrameFromSource();
		return false; // ToDo: save *.aseprite
	}

	std::vector<uint32_t> RGBA;
	UI::ZXIndexColorToRGBA(RGBA, ZXColorView->IndexedData, Width, Height);

	const std::filesystem::path PNGFilePath = IO::NormalizePath(SavePath / std::format("{}.png", SaveName.string()));
	constexpr int32_t Channels = 4;
	if (!stbi_write_png(PNGFilePath.string().c_str(), Width, Height, Channels, RGBA.data(), Width * Channels))
	{
		std::cerr << "Failed to write PNG!" << std::endl;
		LOG_ERROR("[{}]\t Failed to write PNG!", (__FUNCTION__));
		return false;
	}
	return true;
}

bool SCanvas::SaveIPM(const std::filesystem::path& SavePath, const std::filesystem::path& SaveName)
{
	if (ImageFormat == EImageFormat::Aseprite)
	{
		return SaveAsepriteFrameOverride(SelectedSpritesFrame, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData);
	}

	bool bSaved = true;
	if (ZXColorView->InkData.size() > 0)
	{
		const std::filesystem::path InkDataFilePath = IO::NormalizePath(std::filesystem::absolute(SavePath / std::format("{}.ink", SaveName.string())));
		bSaved &= !IO::SaveBinaryData(ZXColorView->InkData, InkDataFilePath, false);
	}

	if (ZXColorView->AttributeData.size() > 0)
	{
		const std::filesystem::path AttributeDataFilePath = IO::NormalizePath(std::filesystem::absolute(SavePath / std::format("{}.attr", SaveName.string())));
		bSaved &= !IO::SaveBinaryData(ZXColorView->AttributeData, AttributeDataFilePath, false);
	}

	if (ZXColorView->MaskData.size() > 0)
	{
		const std::filesystem::path MaskDataFilePath = IO::NormalizePath(std::filesystem::absolute(SavePath / std::format("{}.mask", SaveName.string())));
		bSaved &= !IO::SaveBinaryData(ZXColorView->MaskData, MaskDataFilePath, false);
	}

	if (!bSaved)
	{
		LOG_ERROR("[{}]\t Failed to save one or more IPM override files.", (__FUNCTION__));
	}
	return bSaved;
}

bool SCanvas::Load(const std::filesystem::path& LoadPath, const std::filesystem::path& LoadName, bool bLoadImage /*= true*/)
{
	if (bLoadImage)
	{
		uint8_t* ImageData = FImageBase::LoadToMemory(SourcePathFile, Width, Height);
		UI::QuantizeToZX(ImageData, Width, Height, 4, ZXColorView->IndexedData, TransparentColor);
		UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);
		ConversionToZX(ConversationSettings);
		FImageBase::ReleaseLoadedIntoMemory(ImageData);
	}

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
	UI::ZXIndexColorToZXAttributeColor(ZXColorView->IndexedData, Width, Height, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData, Settings);
	UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height);
}

void SCanvas::ConversionToCanvas(const UI::FConversationSettings& Settings)
{
	UI::ZXAttributeColorToZXIndexColor(ZXColorView->Image, Width, Height, 
		ZXColorView->IndexedData, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData, 
		AsepriteSprite && !AsepriteSprite->InkLayer.empty());
}

bool SCanvas::UpdateAsepriteFrameFromSource()
{
	if (!AsepriteSprite ||
		SelectedSpritesFrame < 0 ||
		SelectedSpritesFrame >= static_cast<int32_t>(AsepriteSprite->Frames.size()))
	{
		return false;
	}

	std::vector<uint32_t> RGBA;
	UI::ZXIndexColorToRGBA(RGBA, ZXColorView->IndexedData, Width, Height);
	std::vector<uint8_t>& Frame = AsepriteSprite->Frames[SelectedSpritesFrame];
	Frame.resize(RGBA.size() * sizeof(uint32_t));
	std::memcpy(Frame.data(), RGBA.data(), Frame.size());
	return true;
}

void SCanvas::NotifySpritesUpdated(
	int32_t Frame,
	const std::vector<uint8_t>& IndexedData,
	const std::vector<uint8_t>& InkData,
	const std::vector<uint8_t>& AttributeData,
	const std::vector<uint8_t>& MaskData)
{
	FEvent_Sprite Event;
	Event.Tag = FEventTag::UpdateSpriteTag;
	Event.CanvasWidth = Width;
	Event.CanvasHeight = Height;
	Event.SourcePathFile = SourcePathFile.empty()
		? std::filesystem::path(GetWindowWName())
		: SourcePathFile;
	Event.IndexedData = IndexedData;
	Event.InkData = InkData;
	Event.AttributeData = AttributeData;
	Event.MaskData = MaskData;
	Event.AsepriteIndex = Frame;
	if (ImageFormat == EImageFormat::Aseprite && AsepriteSprite)
	{
		Event.InkLayer = AsepriteSprite->InkLayer;
		Event.AttributeLayer = AsepriteSprite->AttributeLayer;
		Event.MaskLayer = AsepriteSprite->MaskLayer;
	}
	SendEvent(Event);
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
	UndoQueue.SetWithUndo(std::make_shared<Undo::TAction<FPixelToCanvas>>(std::bind(&ThisClass::UndoSwapPixel, this, std::placeholders::_1),Pixel));
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

void SCanvas::InvertZXDataInRect(
	std::vector<uint8_t>& InkData,
	std::vector<uint8_t>& AttributeData,
	const ImRect& Rect,
	bool bInvertPixels,
	bool bInvertAttributes) const
{
	const int32_t BoundaryX = Width >> 3;
	const int32_t MinX = ImClamp(static_cast<int32_t>(floorf(Rect.Min.x)), 0, Width);
	const int32_t MinY = ImClamp(static_cast<int32_t>(floorf(Rect.Min.y)), 0, Height);
	const int32_t MaxX = ImClamp(static_cast<int32_t>(ceilf(Rect.Max.x)), 0, Width);
	const int32_t MaxY = ImClamp(static_cast<int32_t>(ceilf(Rect.Max.y)), 0, Height);
	if (BoundaryX <= 0 || MinX >= MaxX || MinY >= MaxY)
	{
		return;
	}

	if (bInvertPixels)
	{
		for (int32_t Y = MinY; Y < MaxY; ++Y)
		{
			for (int32_t X = MinX; X < MaxX; ++X)
			{
				const int32_t PixelIndex = Y * BoundaryX + (X >> 3);
				if (PixelIndex >= 0 && PixelIndex < static_cast<int32_t>(InkData.size()))
				{
					InkData[PixelIndex] ^= static_cast<uint8_t>(1 << (7 - (X & 7)));
				}
			}
		}
	}

	if (bInvertAttributes)
	{
		const int32_t MinAttributeX = MinX >> 3;
		const int32_t MinAttributeY = MinY >> 3;
		const int32_t MaxAttributeX = (MaxX + 7) >> 3;
		const int32_t MaxAttributeY = (MaxY + 7) >> 3;
		for (int32_t AttributeY = MinAttributeY; AttributeY < MaxAttributeY; ++AttributeY)
		{
			for (int32_t AttributeX = MinAttributeX; AttributeX < MaxAttributeX; ++AttributeX)
			{
				const int32_t AttributeIndex = AttributeY * BoundaryX + AttributeX;
				if (AttributeIndex < 0 || AttributeIndex >= static_cast<int32_t>(AttributeData.size()))
				{
					continue;
				}

				const uint8_t Attribute = AttributeData[AttributeIndex];
				AttributeData[AttributeIndex] = static_cast<uint8_t>(
					(Attribute & 0xC0) |
					((Attribute & 0x07) << 3) |
					((Attribute >> 3) & 0x07));
			}
		}
	}
}

bool SCanvas::ApplyFrameInversion()
{
	FrameInversionError.clear();
	if (!bInvertFramePixels && !bInvertFrameAttributes)
	{
		return false;
	}
	if (!AsepriteSprite || !AsepriteSprite->IsValid())
	{
		FrameInversionError = "Нет загруженной анимации Aseprite.";
		return false;
	}
	if (FrameInversionRect.GetWidth() <= 0.0f || FrameInversionRect.GetHeight() <= 0.0f)
	{
		FrameInversionError = "Сначала выделите область на Canvas.";
		return false;
	}

	if (bIPMDirty && !SaveAsepriteFrameOverride(
		SelectedSpritesFrame,
		ZXColorView->InkData,
		ZXColorView->AttributeData,
		ZXColorView->MaskData))
	{
		FrameInversionError = "Не удалось сохранить текущие изменения кадра.";
		return false;
	}

	const int32_t FirstFrame = bInvertAllFrames ? 0 : SelectedSpritesFrame;
	const int32_t LastFrame = bInvertAllFrames
		? static_cast<int32_t>(AsepriteSprite->Frames.size())
		: SelectedSpritesFrame + 1;

	for (int32_t Frame = FirstFrame; Frame < LastFrame; ++Frame)
	{
		std::vector<uint8_t> InkData;
		std::vector<uint8_t> AttributeData;
		std::vector<uint8_t> MaskData;
		if (!BuildAsepriteFrameZXData(Frame, InkData, AttributeData, MaskData))
		{
			FrameInversionError = std::format("Не удалось подготовить кадр {}.", Frame);
			return false;
		}

		InvertZXDataInRect(
			InkData,
			AttributeData,
			FrameInversionRect,
			bInvertFramePixels,
			bInvertFrameAttributes);
		if (!SaveAsepriteFrameOverride(Frame, InkData, AttributeData, MaskData))
		{
			FrameInversionError = std::format("Не удалось сохранить кадр {}.", Frame);
			return false;
		}

		std::vector<uint8_t> IndexedData;
		ConvertZXDataToIndexed(
			Width,
			Height,
			InkData,
			AttributeData,
			MaskData,
			IndexedData);
		NotifySpritesUpdated(Frame, IndexedData, InkData, AttributeData, MaskData);
	}

	bIPMDirty = false;
	bNeedConvertZXToCanvas = true;
	LastRebuiltSpriteFrame = INDEX_NONE;
	bFroceRebuiltSpriteFrame = true;
	bRefreshCanvas = true;
	return true;
}

void SCanvas::ChangeFrameMode(EFrameMode::Type NewFrameMode)
{
	if (FrameMode == NewFrameMode)
	{
		return;
	}
	FrameMode = NewFrameMode;

	bRefreshCanvas = true;
}

void SCanvas::RebuildCanvasFromAseprite(int32_t Frame /*= 0*/)
{
	const bool bInk = OptionsFlags[0] & FCanvasOptionsFlags::Ink;
	const bool bMask = OptionsFlags[0] & FCanvasOptionsFlags::Mask;
	const bool bPaper = OptionsFlags[0] & FCanvasOptionsFlags::Attribute;
	const bool bSource = OptionsFlags[0] & FCanvasOptionsFlags::Source;
	const bool bTransparentPaper = AsepriteSprite && !AsepriteSprite->InkLayer.empty();
	const bool bDifference = FrameMode == EFrameMode::Difference || FrameMode == EFrameMode::ReverseDifference;
	const bool bReverseDifference = FrameMode == EFrameMode::ReverseDifference;
	const bool bValidAsepriteFrame = AsepriteSprite &&
		AsepriteSprite->IsValid() &&
		Frame >= 0 &&
		Frame < static_cast<int32_t>(AsepriteSprite->Frames.size());
	const bool bRebuildFrame = bValidAsepriteFrame &&
		(bFroceRebuiltSpriteFrame || LastRebuiltSpriteFrame != Frame);

	if (bRebuildFrame)
	{
		// Invalidate merge snapshots when another frame or layer state is loaded.
		++ZXDataGeneration;
		UI::QuantizeToZX(AsepriteSprite->Frames[Frame].data(), Width, Height, 4, ZXColorView->IndexedData, TransparentColor);
		UI::ZXIndexColorToZXAttributeColor(
			ZXColorView->IndexedData,
			Width, Height,
			ZXColorView->InkData,
			ZXColorView->AttributeData,
			ZXColorView->MaskData,
			ConversationSettings);
		LoadAsepriteFrameOverride(Frame, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData);
		ApplyAsepriteLayerOverrides(Frame, ZXColorView->InkData, ZXColorView->AttributeData, ZXColorView->MaskData);
		LastRebuiltSpriteFrame = Frame;
	}

	if (bValidAsepriteFrame && bDifference && Frame != 0)
	{
		if (bSource)
		{
			std::vector<uint8_t> CurrentInkData;
			std::vector<uint8_t> CurrentAttributeData;
			std::vector<uint8_t> CurrentMaskData;
			std::vector<uint8_t> PreviousInkData;
			std::vector<uint8_t> PreviousAttributeData;
			std::vector<uint8_t> PreviousMaskData;
			if (BuildAsepriteFrameZXData(Frame, CurrentInkData, CurrentAttributeData, CurrentMaskData) &&
				BuildAsepriteFrameZXData(Frame - 1, PreviousInkData, PreviousAttributeData, PreviousMaskData))
			{
				std::vector<uint8_t> CurrentFrameIndexedData;
				std::vector<uint8_t> PreviousFrameIndexedData;
				ConvertZXDataToIndexed(Width, Height, CurrentInkData, CurrentAttributeData, CurrentMaskData, CurrentFrameIndexedData);
				ConvertZXDataToIndexed(Width, Height, PreviousInkData, PreviousAttributeData, PreviousMaskData, PreviousFrameIndexedData);

				std::vector<uint8_t> DifferenceIndexedData(CurrentFrameIndexedData.size());
				for (int32_t Index = 0; Index < static_cast<int32_t>(DifferenceIndexedData.size()); ++Index)
				{
					const uint8_t CurrentColor = CurrentFrameIndexedData[Index];
					const uint8_t PreviousColor = PreviousFrameIndexedData[Index];
					DifferenceIndexedData[Index] = CurrentColor != PreviousColor
						? (bReverseDifference ? PreviousColor : CurrentColor)
						: EZXColor::Transparent;
				}
				UI::ZXIndexColorToImage(ZXColorView->Image, DifferenceIndexedData, Width, Height);
			}
		}
		else
		{
			std::vector<uint8_t> DifferenceInkData(ZXColorView->InkData.size());
			std::vector<uint8_t> DifferenceAttributeData(ZXColorView->AttributeData.size());
			std::vector<uint8_t> DifferenceMaskData(ZXColorView->MaskData.size());
			if (FrameDifferenceZXColor(Frame, DifferenceInkData, DifferenceAttributeData, DifferenceMaskData, bReverseDifference))
			{
				UI::ZXAttributeColorToImage(
					ZXColorView->Image,
					Width, Height,
					(bTransparentMask || bInk) ? DifferenceInkData.data() : nullptr,
					(bTransparentMask || bPaper) ? DifferenceAttributeData.data() : nullptr,
					bMask ? DifferenceMaskData.data() : nullptr,
					false, nullptr, true,
					bTransparentMask,
					bTransparentPaper);
			}
		}
		bFroceRebuiltSpriteFrame = false;
		return;
	}

	if (!bSource && bAttributeClash && FrameMode == EFrameMode::None)
	{
		std::vector<uint8_t> FinalZXIndexedData;
		ConvertZXDataToIndexed(
			Width,
			Height,
			ZXColorView->InkData,
			ZXColorView->AttributeData,
			ZXColorView->MaskData,
			FinalZXIndexedData);

		// Black and Transparent share index 0. Canonicalize opaque black so it
		// compares equal to the opaque black produced by source quantization.
		const int32_t BoundaryX = Width >> 3;
		for (int32_t Y = 0; Y < Height; ++Y)
		{
			for (int32_t X = 0; X < Width; ++X)
			{
				const int32_t ByteX = X >> 3;
				if (ByteX >= BoundaryX)
				{
					continue;
				}
				const size_t MaskIndex = static_cast<size_t>(Y) * BoundaryX + ByteX;
				const size_t PixelIndex = static_cast<size_t>(Y) * Width + X;
				const uint8_t PixelBit = static_cast<uint8_t>(1 << (7 - (X & 7)));
				if (MaskIndex < ZXColorView->MaskData.size() &&
					PixelIndex < FinalZXIndexedData.size() &&
					(ZXColorView->MaskData[MaskIndex] & PixelBit) != 0 &&
					FinalZXIndexedData[PixelIndex] == EZXColor::Black)
				{
					FinalZXIndexedData[PixelIndex] = EZXColor::Black_;
				}
			}
		}

		if (bLastAttributeClashPhase)
		{
			const size_t PixelCount = min(FinalZXIndexedData.size(), ZXColorView->IndexedData.size());
			for (size_t PixelIndex = 0; PixelIndex < PixelCount; ++PixelIndex)
			{
				if (ZXColorView->IndexedData[PixelIndex] != FinalZXIndexedData[PixelIndex])
				{
					FinalZXIndexedData[PixelIndex] = ZXColorView->IndexedData[PixelIndex];
				}
			}
		}

		UI::ZXIndexColorToImage(ZXColorView->Image, FinalZXIndexedData, Width, Height);
		bFroceRebuiltSpriteFrame = false;
		return;
	}

	if (bSource)
	{
		UI::ZXIndexColorToImage(ZXColorView->Image, ZXColorView->IndexedData, Width, Height);
	}
	else
	{
		UI::ZXAttributeColorToImage(
			ZXColorView->Image,
			Width, Height,
			(bTransparentMask || bInk) ? ZXColorView->InkData.data() : nullptr,
			(bTransparentMask || bPaper) ? ZXColorView->AttributeData.data() : nullptr,
			bMask ? ZXColorView->MaskData.data() : nullptr,
			false, nullptr, true,
			bTransparentMask,
			bTransparentPaper);
	}
	bFroceRebuiltSpriteFrame = false;
}

bool SCanvas::FrameDifferenceZXColor(
	int32_t Frame,
	std::vector<uint8_t>& OutputDifference_InkData,
	std::vector<uint8_t>& OutputDifference_AttributeData,
	std::vector<uint8_t>& OutputDifference_MaskData,
	bool bReverse)
{
	// current frame
	std::vector<uint8_t> CurrentFrame_InkData;
	std::vector<uint8_t> CurrentFrame_AttributeData;
	std::vector<uint8_t> CurrentFrame_MaskData;
	if (!BuildAsepriteFrameZXData(
		Frame,
		CurrentFrame_InkData,
		CurrentFrame_AttributeData,
		CurrentFrame_MaskData))
	{
		return false;
	}

	if (Frame == 0)
	{
		OutputDifference_InkData = CurrentFrame_InkData;
		OutputDifference_AttributeData = CurrentFrame_AttributeData;
		OutputDifference_MaskData = CurrentFrame_MaskData;
		return true;
	}

	// previous frame
	std::vector<uint8_t> PreviousFrame_InkData;
	std::vector<uint8_t> PreviousFrame_AttributeData;
	std::vector<uint8_t> PreviousFrame_MaskData;
	if (!BuildAsepriteFrameZXData(
		Frame - 1,
		PreviousFrame_InkData,
		PreviousFrame_AttributeData,
		PreviousFrame_MaskData))
	{
		return false;
	}


	const int32_t Boundary_X = Width >> 3;

	const int32_t Size_6912 = Boundary_X * Height;
	for (int32_t Index = 0; Index < Size_6912; ++Index)
	{
		// ink
		const uint8_t Current_InkData = CurrentFrame_InkData[Index];
		const uint8_t Previous_InkData = PreviousFrame_InkData[Index];
		const bool bDifference_Ink = Current_InkData ^ Previous_InkData;

		OutputDifference_InkData[Index] = bDifference_Ink
			? (bReverse ? Previous_InkData : Current_InkData)
			: EZXColor::Transparent;
		OutputDifference_MaskData[Index] = bDifference_Ink ? 0xFF : 0x00;

		// attribute
		const int32_t bx = Index % Boundary_X;
		const int32_t by = (Index / Boundary_X) / 8;
		const int32_t Index_Attribute = by * Boundary_X + bx;
		const uint8_t Current_AttributeData = CurrentFrame_AttributeData[Index_Attribute];
		const uint8_t Previous_AttributeData = PreviousFrame_AttributeData[Index_Attribute];
		const bool bDifference_Attribute = Current_AttributeData ^ Previous_AttributeData;

		OutputDifference_AttributeData[Index_Attribute] = bDifference_Attribute
			? (bReverse ? Previous_AttributeData : Current_AttributeData)
			: 0xFF;
	}

	return true;
}

void SCanvas::UndoSwapPixel(FPixelToCanvas& Param)
{
	//std::swap(OptionsFlags[0], Param.Canvas);

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
		bSourceDirty = true;
	}
	else
	{
		bIPMDirty = true;
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
			uint8_t _Attribute = Attribute;

			// swap pixel color
			const uint8_t PixelBit = 1 << (7 - dx);
			const uint8_t Flags = OptionsFlags[0] & ~FCanvasOptionsFlags::Source;

			// swap pixel bit
			if (/*Flags & FCanvasOptionsFlags::Ink && */Subcolor[ESubcolor::Ink] != EZXColor::Transparent)
			{
				uint8_t& PixelsByte = reinterpret_cast<uint8_t*>(&Color)[3];
				uint8_t Diff = (Pixels ^ PixelsByte) & PixelBit;
				Pixels ^= Diff;
				PixelsByte ^= Diff;
			}
			// swap mask bit
			if (/*Flags & FCanvasOptionsFlags::Mask && */Subcolor[ESubcolor::Ink] != EZXColor::Transparent)
			{
				uint8_t& MaskByte = reinterpret_cast<uint8_t*>(&Color)[2];
				uint8_t Diff = (Mask ^ MaskByte) & PixelBit;
				Mask ^= Diff;
				MaskByte ^= Diff;
			}
			// swap byte attribute
			if (/*Flags & FCanvasOptionsFlags::Attribute && */Subcolor[ESubcolor::Paper] != EZXColor::Transparent)
			{
				std::swap(Attribute, reinterpret_cast<uint8_t*>(&Color)[1]);
			}

			// set pixel color
			uint8_t& _Color = reinterpret_cast<uint8_t*>(&Color)[0];
			if (_Color != EZXColor::None)
			{
				if (bTransparentMask || Flags & FCanvasOptionsFlags::Ink)
				{
					if (_Color != EZXColor::Transparent)
					{
						const uint8_t PixelBit = 1 << (7 - dx);
						const bool bOperation = (_Color & 0x07) != EZXColor::White;
						if (!bTransparentMask && bOperation)
						{
							Pixels |= PixelBit;									// set bit
						}
						else
						{
							Pixels &= ~(PixelBit);								// reset bit
						}
					}
				}
				if (bTransparentMask || Flags & FCanvasOptionsFlags::Mask)
				{
					const uint8_t PixelBit = 1 << (7 - dx);
					const bool bOperation = _Color != EZXColor::Transparent;
					if (bOperation)
					{
						Mask |= PixelBit;										// set bit
					}
					else
					{
						Mask &= ~(PixelBit);									// reset bit
					}
				}
				if (bTransparentMask || Flags & FCanvasOptionsFlags::Attribute)
				{
					const bool bInkTransparent = Subcolor[ESubcolor::Ink] == EZXColor::Transparent;
					const bool bPaperTransparent = Subcolor[ESubcolor::Paper] == EZXColor::Transparent;

					const uint8_t InkColor = bInkTransparent ? (Attribute & 0x07) : Subcolor[ESubcolor::Ink] & 0x07;
					const uint8_t PaperColor = bPaperTransparent ? ((Attribute >> 3) & 0x07) : Subcolor[ESubcolor::Paper] & 0x07;

					const bool bBright = bTransparentMask ? (_Attribute & 0x40) : Subcolor[ESubcolor::Bright] == EZXColor::True;
					const bool bFlash = bTransparentMask ? (_Attribute & 0x80) : Subcolor[ESubcolor::Flash] == EZXColor::True;

					Attribute = (bFlash << 7) | (bBright << 6) | (PaperColor << 3) | InkColor;
				}

				_Color = EZXColor::None;
			}
		}
		bNeedConvertZXToCanvas = true;
	}
	bRefreshCanvas = true;
}

bool SCanvas::SplitSpriteName(const std::string& Name, std::string& Base, int32_t& Number) const
{
	if (Name.empty())
	{
		return false;
	}

	int32_t Index = (int32_t)Name.size() - 1;

	// start from the end while there are numbers
	while (Index >= 0 && std::isdigit(static_cast<unsigned char>(Name[Index])))
	{
		--Index;
	}

	// if there are no numbers
	if (Index == (int32_t)Name.size() - 1)
	{
		Base = Name;
		Number = 0;
		return true;
	}

	Base = Name.substr(0, Index + 1);
	Number = std::stoi(Name.substr(Index + 1));

	return true;
}

std::vector<SCanvas::FSpriteNameOption> SCanvas::GetSpriteNameOptions() const
{
	std::map<std::string, FSpriteNameOption> OptionsByBase;

	for (const auto& [ID, Name] : SpriteNames)
	{
		int32_t Number;
		std::string Base;
		if (SplitSpriteName(Name, Base, Number))
		{
			FSpriteNameOption& Option = OptionsByBase[Base];
			Option.Base = Base;
			++Option.Count;
			Option.MaxNumber = max(Option.MaxNumber, Number);
		}
	}

	std::vector<FSpriteNameOption> Options;
	Options.reserve(OptionsByBase.size());
	for (auto& [Base, Option] : OptionsByBase)
	{
		Options.push_back(std::move(Option));
	}
	std::sort(Options.begin(), Options.end(),
		[](const FSpriteNameOption& Left, const FSpriteNameOption& Right)
		{
			if (Left.Count != Right.Count)
			{
				return Left.Count > Right.Count;
			}
			return Left.Base < Right.Base;
		});
	return Options;
}

std::string SCanvas::GetNextSpriteName(const std::vector<FSpriteNameOption>& Options, const std::string& Base /*= ""*/)
{
	const auto Selected = Base.empty()
		? Options.end()
		: std::find_if(Options.begin(), Options.end(), [&Base](const FSpriteNameOption& Option) { return Option.Base == Base; });
	if (Selected != Options.end())
	{
		return Selected->Base + std::to_string(Selected->MaxNumber + 1);
	}
	if (!Options.empty())
	{
		return Options.front().Base + std::to_string(Options.front().MaxNumber + 1);
	}
	if (!Base.empty())
	{
		return Base + "0";
	}
	else
	{
		return std::format("Sprite {}", ++SpriteCounter);
	}
}

CodeGenerator::FResult SCanvas::BuildCodeGenerationResult(const CodeGenerator::FOptions& Options, const std::string& LabelName, const CodeGenerator::FProgressInfo* Progress)
{
	CodeGenerator::FResult Result;
	CodeGenerator::FOptions EffectiveOptions = Options;
	auto HasFrameOverride = [this](int32_t Frame)
	{
		if (Frame < 0)
		{
			return false;
		}
		const std::filesystem::path InkPath = GetAsepriteFrameOverridePath(Frame, ".ink");
		const std::filesystem::path AttributePath = GetAsepriteFrameOverridePath(Frame, ".attr");
		const std::filesystem::path MaskPath = GetAsepriteFrameOverridePath(Frame, ".mask");
		return (!InkPath.empty() && std::filesystem::exists(InkPath)) ||
			(!AttributePath.empty() && std::filesystem::exists(AttributePath)) ||
			(!MaskPath.empty() && std::filesystem::exists(MaskPath));
	};
	EffectiveOptions.CurrentFrameOverride = HasFrameOverride(SelectedSpritesFrame);
	EffectiveOptions.PreviousFrameOverride = HasFrameOverride(SelectedSpritesFrame - 1);
	if (!Options.GeneratePixels && !Options.GenerateAttributes)
	{
		Result.Error = "Code generation: pixels and attributes are both disabled";
		return Result;
	}
	if (Options.ReverseFrameDifference && SelectedSpritesFrame == 0)
	{
		Result.Error = "Code generation: reverse difference requires frame 1 or later";
		return Result;
	}
	std::vector<uint8_t> Difference_InkData(CodeGenerator::ZX_PIXEL_SIZE);
	std::vector<uint8_t> Difference_AttributeData(CodeGenerator::ZX_ATTRIBUTE_SIZE);
	std::vector<uint8_t> Difference_MaskData(CodeGenerator::ZX_PIXEL_SIZE);
	if (!FrameDifferenceZXColor(
		SelectedSpritesFrame,
		Difference_InkData,
		Difference_AttributeData,
		Difference_MaskData,
		Options.ReverseFrameDifference))
	{
		Result.Error = std::format("Error: getting difference in ZX frame Color");
		return Result;
	}
	const std::string EffectiveLabelName = LabelName.empty() ? CodeGenerator::MakeFrameLabelName(SelectedSpritesFrame) : LabelName;
	return CodeGeneration(Difference_InkData, Difference_AttributeData, Difference_MaskData, EffectiveOptions, EffectiveLabelName, Progress);
}

CodeGenerator::FResult SCanvas::CodeGeneration(
	const std::vector<uint8_t>& InkData,
	const std::vector<uint8_t>& AttributeData,
	const std::vector<uint8_t>& MaskData,
	const CodeGenerator::FOptions& Options,
	const std::string& LabelName,
	const CodeGenerator::FProgressInfo* Progress)
{
	CodeGenerator::FResult Result;

	/*
	DirtyMask — это дифф-маска, то есть карта байтов, которые надо реально перезаписать.
		DirtyMask[i] = 1; // этот байт надо записать
		DirtyMask[i] = 0; // этот байт пропускаем
	*/
	// Пока прозрачности нет — DirtyMask пустой.
	std::vector<uint8_t> ScreenData(CodeGenerator::ZX_SCREEN_SIZE);
	std::vector<uint8_t> DirtyMask(CodeGenerator::ZX_SCREEN_SIZE);
	std::vector<uint8_t> PixelAllowedMask(CodeGenerator::ZX_PIXEL_SIZE, 1);
	std::vector<uint8_t> AttributeAllowedMask(CodeGenerator::ZX_ATTRIBUTE_SIZE, 1);

	int32_t SourceMinX = 0;
	int32_t SourceMinY = 0;
	int32_t SourceMaxX = 256;
	int32_t SourceMaxY = 192;
	CodeGenerator::FOptions EffectiveOptions = Options;
	EffectiveOptions.DestinationX = ImClamp(Options.DestinationX, 0, 255) & ~7;
	EffectiveOptions.DestinationY = ImClamp(Options.DestinationY, 0, 191);
	int32_t DestinationX = EffectiveOptions.DestinationX;
	int32_t DestinationY = EffectiveOptions.DestinationY;
	if (Options.ProjectSelection)
	{
		if (!ZXColorView || !ZXColorView->bVisibilityRectangleMarquee ||
			ZXColorView->RectangleMarqueeRect.GetWidth() <= 0.0f ||
			ZXColorView->RectangleMarqueeRect.GetHeight() <= 0.0f)
		{
			Result.Error = "Code generation: project selection requires an active canvas selection";
			return Result;
		}

		const ImRect& Selection = ZXColorView->RectangleMarqueeRect;
		SourceMinX = ImClamp(static_cast<int32_t>(floorf(Selection.Min.x)), 0, 255) & ~7;
		SourceMinY = ImClamp(static_cast<int32_t>(floorf(Selection.Min.y)), 0, 191);
		SourceMaxX = ImClamp((static_cast<int32_t>(ceilf(Selection.Max.x)) + 7) & ~7, 0, 256);
		SourceMaxY = ImClamp(static_cast<int32_t>(ceilf(Selection.Max.y)), 0, 192);
		EffectiveOptions.HasSelectionSource = true;
		EffectiveOptions.SelectionSourceMinX = SourceMinX;
		EffectiveOptions.SelectionSourceMinY = SourceMinY;
		EffectiveOptions.SelectionSourceMaxX = SourceMaxX;
		EffectiveOptions.SelectionSourceMaxY = SourceMaxY;
	}

	if (Keyframes && AsepriteSprite && AsepriteSprite->IsValid())
	{
		bool bHasCodeGenerationMask = false;
		std::vector<uint8_t> LimitedPixelAllowedMask(CodeGenerator::ZX_PIXEL_SIZE, 0);
		std::vector<uint8_t> LimitedAttributeAllowedMask(CodeGenerator::ZX_ATTRIBUTE_SIZE, 0);

		for (int32_t LayerIndex = 0; LayerIndex < (int32_t)AsepriteSprite->Layers.size(); ++LayerIndex)
		{
			const FPropertyBag& Property = Keyframes->GetProperty(SelectedSpritesFrame, LayerIndex);
			if (!Property.IsValid())
			{
				continue;
			}

			std::vector<uint8_t> LayerPixelAllowedMask(CodeGenerator::ZX_PIXEL_SIZE, 0);
			std::vector<uint8_t> LayerAttributeAllowedMask(CodeGenerator::ZX_ATTRIBUTE_SIZE, 0);
			bool bHasLayerMask = false;

			FTilemapCellData_Rect LimitArea;
			if (Property.GetStruct(FPropertyTag::LimitArea, LimitArea))
			{
				if (LimitArea.bActiveArea)
				{
					FTilemapCellData_ByteValues IgnoredPixels;
					Property.GetStruct(FPropertyTag::ActiveAreaIgnoredPixels, IgnoredPixels);
					ApplyActiveAreaToCodeGenerationMask(
						LimitArea.Rect,
						Width,
						Height,
						IgnoredPixels,
						InkData,
						AttributeData,
						MaskData,
						LayerPixelAllowedMask,
						LayerAttributeAllowedMask);
					bHasLayerMask = true;
				}
				else
				{
					FTilemapCellData_Rect LimitArea;
					if (Property.GetStruct(FPropertyTag::LimitArea, LimitArea))
					{
						ApplyLimitAreaToCodeGenerationMask(
							LimitArea.Rect,
							Width,
							Height,
							LayerPixelAllowedMask,
							LayerAttributeAllowedMask);
						bHasLayerMask = true;
					}
				}
			}

			if (!bHasLayerMask)
			{
				continue;
			}

			for (int32_t PixelIndex = 0; PixelIndex < CodeGenerator::ZX_PIXEL_SIZE; ++PixelIndex)
			{
				LimitedPixelAllowedMask[PixelIndex] = LimitedPixelAllowedMask[PixelIndex] || LayerPixelAllowedMask[PixelIndex] ? 1 : 0;
			}
			for (int32_t AttrIndex = 0; AttrIndex < CodeGenerator::ZX_ATTRIBUTE_SIZE; ++AttrIndex)
			{
				LimitedAttributeAllowedMask[AttrIndex] = LimitedAttributeAllowedMask[AttrIndex] || LayerAttributeAllowedMask[AttrIndex] ? 1 : 0;
			}
			bHasCodeGenerationMask = true;
		}

		if (bHasCodeGenerationMask)
		{
			PixelAllowedMask = std::move(LimitedPixelAllowedMask);
			AttributeAllowedMask = std::move(LimitedAttributeAllowedMask);
		}
	}

	// fill pixels
	if (Options.GeneratePixels)
	{
		for (int32_t PixelIndex = 0; PixelIndex < CodeGenerator::ZX_PIXEL_SIZE; ++PixelIndex)
		{
			const int32_t SourceY = PixelIndex / 32;
			const int32_t SourceByteX = PixelIndex % 32;
			const int32_t SourceX = SourceByteX * 8;
			if (SourceX < SourceMinX || SourceX >= SourceMaxX || SourceY < SourceMinY || SourceY >= SourceMaxY)
			{
				continue;
			}

			const int32_t TargetByteX = Options.ProjectSelection ? DestinationX / 8 + (SourceX - SourceMinX) / 8 : SourceByteX;
			const int32_t TargetY = Options.ProjectSelection ? DestinationY + SourceY - SourceMinY : SourceY;
			if (TargetByteX < 0 || TargetByteX >= 32 || TargetY < 0 || TargetY >= 192)
			{
				continue;
			}

			const uint8_t& Mask = MaskData[PixelIndex];
			const uint8_t& Pixels = InkData[PixelIndex];
			const int32_t ZXPixelOffset = GetZXScreenPixelOffset(TargetY * 32 + TargetByteX);
			const bool bAllowed = PixelAllowedMask[PixelIndex] != 0;

			DirtyMask[ZXPixelOffset] = Mask == 0x00 || !bAllowed ? 0 : 1;
			ScreenData[ZXPixelOffset] = Mask == 0x00 || !bAllowed ? 0 : Pixels;
		}
	}

	// fill attribute
	if (Options.GenerateAttributes)
	{
		const int32_t SourceMinAttrX = SourceMinX / 8;
		const int32_t SourceMinAttrY = SourceMinY / 8;
		const int32_t SourceMaxAttrX = (SourceMaxX + 7) / 8;
		const int32_t SourceMaxAttrY = (SourceMaxY + 7) / 8;
		for (int32_t AttrIndex = 0; AttrIndex < CodeGenerator::ZX_ATTRIBUTE_SIZE; ++AttrIndex)
		{
			const int32_t SourceAttrX = AttrIndex % 32;
			const int32_t SourceAttrY = AttrIndex / 32;
			if (SourceAttrX < SourceMinAttrX || SourceAttrX >= SourceMaxAttrX ||
				SourceAttrY < SourceMinAttrY || SourceAttrY >= SourceMaxAttrY)
			{
				continue;
			}

			const int32_t TargetAttrX = Options.ProjectSelection ? DestinationX / 8 + SourceAttrX - SourceMinAttrX : SourceAttrX;
			const int32_t TargetAttrY = Options.ProjectSelection ? DestinationY / 8 + SourceAttrY - SourceMinAttrY : SourceAttrY;
			if (TargetAttrX < 0 || TargetAttrX >= 32 || TargetAttrY < 0 || TargetAttrY >= 24)
			{
				continue;
			}

			const uint8_t& Attribute = AttributeData[AttrIndex];
			const bool bAllowed = AttributeAllowedMask[AttrIndex] != 0;
			const int32_t TargetAttrIndex = TargetAttrY * 32 + TargetAttrX;

			DirtyMask[CodeGenerator::ZX_PIXEL_SIZE + TargetAttrIndex] = Attribute == 0xFF || !bAllowed ? 0 : 1;
			ScreenData[CodeGenerator::ZX_PIXEL_SIZE + TargetAttrIndex] = Attribute == 0xFF || !bAllowed ? 0 : Attribute;
		}
	}

	CodeGenerator::FAnalysis Analysis;
	if (!BuildAnalysis(ScreenData, DirtyMask, EffectiveOptions, Analysis, Result.Error, Progress))
	{
		return Result;
	}

	CodeGenerator::FPlan Plan;
	if (!OptimizePlan(Analysis, EffectiveOptions, Plan, Result.Error, Progress))
	{
		return Result;
	}

	int32_t EmittedCycles = 0;
	if (!EmitAsm(Analysis, Plan, EffectiveOptions, Result.AsmCode, Result.ByteCode, EmittedCycles, Result.Error, LabelName.empty() ? "DrawFrame" : LabelName, Progress))
	{
		return Result;
	}

	Result.OperationCount = (int32_t)Plan.CandidateIds.size();
	Result.Cycles = EmittedCycles;
	Result.CodeBytes = (int32_t)Result.ByteCode.size();
	Result.DirtyBytes = 0;

	for (uint8_t Dirty : Analysis.Dirty)
	{
		Result.DirtyBytes += Dirty ? 1 : 0;
	}

	Result.bSuccess = true;
	if (Result.AsmCode.empty())
	{
		Result.bSuccess = false;
		Result.Error = "Code generation produced empty output.";
	}
	if (Result.ByteCode.empty())
	{
		Result.bSuccess = false;
		Result.Error = "Code generation produced empty bytecode.";
	}

	return Result;
}
