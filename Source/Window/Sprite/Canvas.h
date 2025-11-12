#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include <Core/Image.h>
#include <Utils/UI/Draw_ZXColorVideo.h>
#include "Palette.h"
#include "ToolBar.h"
#include "Definition.h"

namespace FCanvasOptionsFlags
{
	enum Type
	{
		None = 0,
		Source = 1 << 0,
		Ink = 1 << 1,
		Attribute = 1 << 2,
		Mask = 1 << 3,
	};
}

struct FSprite;

class SCanvas : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SCanvas;

public:
	SCanvas(EFont::Type _FontName, const std::wstring& Name = L"", const std::filesystem::path& SourcePathFile = {});
	virtual ~SCanvas() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize(const std::any& Arg) override;
	virtual void SetupHotKeys() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Render() override;
	virtual void Destroy() override;

	const std::filesystem::path& GetSourcePathFile() const { return SourcePathFile; }

private:
	void Draw_PopupMenu();
	void Draw_PopupMenu_CreateSprite();

	void Input_HotKeys();
	void Input_Mouse();

	// set mode
	void SetToolMode(EToolMode::Type NewToolMode, bool bForce = true, bool bEvent = false);
	bool IsEqualToolMode(EToolMode::Type Equal, uint8_t Index = 0) const { return ToolMode[Index] == Equal; }

	void Imput_SetToolMode_None()				{ SetToolMode(EToolMode::None, true);				}
	void Imput_SetToolMode_RectangleMarquee()	{ SetToolMode(EToolMode::RectangleMarquee, true);	}
	void Imput_SetToolMode_Pencil()				{ SetToolMode(EToolMode::Pencil, true);				}
	void Imput_SetToolMode_Eraser()				{ SetToolMode(EToolMode::Eraser, true);				}
	void Imput_SetToolMode_Eyedropper()			{ SetToolMode(EToolMode::Eyedropper, true);			}
	void Imput_SetToolMode_PaintBucket()		{ SetToolMode(EToolMode::PaintBucket, true);		}
	void ApplyToolMode();

	void Imput_SelectAll() {}
	void Imput_Copy() {}
	void Imput_Paste();
	void Imput_Cut() {}
	void Imput_Delete() {}
	void Imput_Undo() {}
	void Imput_Redo() {}

	void Reset_RectangleMarquee();
	void Handler_RectangleMarquee();
	void Handler_Pencil();
	void Handler_Eyedropper();

	void ConversionToZX(const UI::FConversationSettings& Settings);
	void ConversionToCanvas(const UI::FConversationSettings& Settings);
	void Set_PixelToCanvas(const ImVec2& Position, uint8_t ButtonIndex);

	void UpdateCursorColor(bool bButton = false);

	bool bDragging;
	bool bRefreshCanvas;
	bool bRectangleMarqueeActive;
	bool bNeedConvertCanvasToZX;
	bool bNeedConvertZXToCanvas;
	bool bOpenPopupMenu;
	bool bMouseInsideMarquee;

	// popup menu 'New Sprite'
	bool bRoundingToMultipleEight;
	bool bRectangularSprite;

	// popup menu 'Add Meta'
	int32_t IndexSelectedSprites;
	std::vector<std::shared_ptr<FSprite>> SelectedSprites;

	ImGuiID CanvasID;

	// draw pixels
	UI::EZXSpectrumColor::Type ButtonColor[2];
	UI::EZXSpectrumColor::Type Subcolor[ESubcolor::MAX];
	uint8_t LastSetButtonIndex;
	uint8_t LastSetPixelColorIndex;
	ImVec2 LastSetPixelPosition;

	int32_t Width;
	int32_t Height;
	uint32_t OptionsFlags[2];
	uint32_t LastOptionsFlags;

	ImVec2 CreateSpriteSize;
	ImVec2 Log2SpriteSize;
	inline static int32_t SpriteCounter = 0;
	char CreateSpriteNameBuffer[BUFFER_SIZE_INPUT] = "";
	char CreateSpriteWidthBuffer[BUFFER_SIZE_INPUT] = "";
	char CreateSpriteHeightBuffer[BUFFER_SIZE_INPUT] = "";

	// clipboard
	FRGBAImage ClipboardImage;

	std::shared_ptr<UI::FZXColorView> ZXColorView;
	UI::FConversationSettings ConversationSettings;
	EToolMode::Type ToolMode[2];
	std::filesystem::path SourcePathFile;
};
