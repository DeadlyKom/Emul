#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include "Core/Image.h"
#include <Utils/UI/Draw_ZXColorVideo.h>
#include "Palette.h"

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

class SCanvas : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SCanvas;
public:
	SCanvas(EFont::Type _FontName);
	virtual ~SCanvas() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize() override;
	virtual void Render() override;
	virtual void Destroy() override;

private:
	void Input_HotKeys();
	void Input_Mouse();

	void ConversionToZX(const UI::FConversationSettings& Settings);
	void ConversionToCanvas(const UI::FConversationSettings& Settings);
	void Set_PixelToCanvas(const ImVec2& Position, uint8_t ButtonIndex);

	bool bDragging;
	bool bRefreshCanvas;
	bool bNeedConvertCanvasToZX;
	bool bNeedConvertZXToCanvas;
	ImGuiID CanvasID;

	// draw pixels
	uint8_t ButtonColor[2];
	uint8_t SubColor[ESubcolor::MAX];
	uint8_t LastSetButtonIndex;
	uint8_t LastSetPixelColorIndex;
	ImVec2 LastSetPixelPosition;

	int32_t Width;
	int32_t Height;
	uint32_t OptionsFlags[2];
	uint32_t LastOptionsFlags;
	std::shared_ptr<UI::FZXColorView> ZXColorView;
	UI::FConversationSettings ConversationSettings;
};
