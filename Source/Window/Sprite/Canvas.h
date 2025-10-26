#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include "Core/Image.h"
#include <Utils/UI/Draw_ZXColorVideo.h>

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
	bool bPressedSource;
	bool bPressedInk;
	bool bPressedPaper;
	bool bPressedMask;
	int32_t Width;
	int32_t Height;
	ImGuiID CanvasID;
	std::shared_ptr<UI::FZXColorView> ZXColorView;
};
