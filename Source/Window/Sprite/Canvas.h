#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include "Core/Image.h"
#include <Utils/UI/Draw_ZXColorVideo.h>

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

	virtual void Initialize() override;
	virtual void Render() override;
	virtual void Destroy() override;

private:
	void Input_HotKeys();
	void Input_Mouse();

	bool bDragging;
	bool bRefreshCanvas;

	int32_t Width;
	int32_t Height;
	uint32_t OptionsFlags;
	uint32_t LastOptionsFlags;
	ImGuiID CanvasID;
	std::shared_ptr<UI::FZXColorView> ZXColorView;
};
