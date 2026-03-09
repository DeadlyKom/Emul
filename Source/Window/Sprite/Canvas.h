#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include <Core/Image.h>
#include <Utils/UndoQueue.h>
#include <Utils/UI/Draw_ZXColorVideo.h>
#include "Palette.h"
#include "ToolBar.h"
#include "Definition.h"
#include "UndoAction.h"

namespace FCanvasOptionsFlags
{
	enum Type
	{
		None		= 0,
		Source		= 1 << 0,
		Ink			= 1 << 1,
		Attribute	= 1 << 2,
		Mask		= 1 << 3,
	};
}

enum class EImageFormat;
struct FSprite;

class SCanvas : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SCanvas;

public:
	SCanvas(EFont::Type _FontName, const std::wstring& Name = L"", const std::filesystem::path& SourcePathFile = {});
	virtual ~SCanvas() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize(const std::vector<std::any>& Args) override;
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

	void Imput_SelectAll();
	void Imput_Copy() {}
	void Imput_Paste();
	void Imput_Cut() {}
	void Imput_Delete();
	void Imput_Undo();
	void Imput_Redo();
	void Imput_Save();

	void Reset_RectangleMarquee();
	void Handler_RectangleMarquee();
	void Handler_Pencil();
	void Handler_Eyedropper();

	bool Save(const std::filesystem::path& SavePath, const std::filesystem::path& SaveName);
	bool Load(const std::filesystem::path& LoadPath, const std::filesystem::path& LoadName);

	void ConversionToZX(const UI::FConversationSettings& Settings);
	void ConversionToCanvas(const UI::FConversationSettings& Settings);
	void Set_PixelToCanvas(const ImVec2& Position, uint8_t ButtonIndex);

	void UpdateCursorColor(bool bButton = false);

	// undo/redo
	void UndoSwapPixel(FPixelToCanvas& Param);

	bool SplitSpriteName(const std::string& Name, std::string& Base, int32_t& Number);
	std::string GetNextSpriteName(const std::map<int32_t, std::string>& Sprites);

	bool bDirty;
	bool bDragging;
	bool bRefreshCanvas;
	bool bTransparentMask;
	bool bRectangleMarqueeActive;
	bool bNeedConvertCanvasToZX;
	bool bNeedConvertZXToCanvas;
	bool bOpenPopupMenu;
	bool bMouseInsideMarquee;

	// popup menu 'New Sprite'
	bool bRoundingToMultipleEight;
	bool bRectangularSprite;

	// popup menu 'Add Meta'
	std::shared_ptr<FSprite> SelectedSprite;

	ImGuiID CanvasID;

	// draw pixels
	UI::EZXSpectrumColor::Type ButtonColor[2];
	UI::EZXSpectrumColor::Type Subcolor[ESubcolor::MAX];
	uint8_t LastSetButtonIndex;
	uint8_t LastSetPixelColorIndex;
	ImVec2 LastSetPixelPosition;

	int32_t Width;
	int32_t Height;
	ImColor TransparentColor;
	uint32_t OptionsFlags[2];
	uint32_t LastOptionsFlags;

	ImVec2 CreateSpriteSize;
	ImVec2 Log2SpriteSize;
	inline static int32_t SpriteCounter = 0;
	char CreateSpriteNameBuffer[BUFFER_SIZE_INPUT] = "";
	char CreateSpriteWidthBuffer[BUFFER_SIZE_INPUT] = "";
	char CreateSpriteHeightBuffer[BUFFER_SIZE_INPUT] = "";
	std::map<int32_t, std::string> SpriteNames;

	// clipboard
	FRGBAImage ClipboardImage;

	std::shared_ptr<UI::FZXColorView> ZXColorView;
	UI::FConversationSettings ConversationSettings;
	EToolMode::Type ToolMode[2];
	EImageFormat ImageFormat;
	int32_t ImageFrameIndex;
	std::filesystem::path SourcePathFile;

	// Undo/Redo
	Undo::FQueue UndoQueue;
	size_t PixelStrokeBegin;
};
