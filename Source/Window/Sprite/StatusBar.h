#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>

class SStatusBar : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SStatusBar;
public:
	SStatusBar(EFont::Type _FontName);
	virtual ~SStatusBar() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize() override;
	virtual void Render() override;

private:
	void Draw_MousePosition();

	ImVec2 CanvasSize;
	ImVec2 MousePosition;
};
