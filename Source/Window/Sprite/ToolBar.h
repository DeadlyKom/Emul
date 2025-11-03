#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include <Core/Image.h>

namespace EToolMode
{
	enum Type
	{
		None,
		RectangleMarquee,		// IDB_RECTANGLE_MARQUEE
		Pencil,					// IDB_PENCIL
		Eraser,					// IDB_ERASER
		Eyedropper,				// IDB_EYEDROPPER
		PaintBucket,			// IDB_PAINTBUCKET
	};
}

class SToolBar : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SToolBar;
public:
	SToolBar(EFont::Type _FontName, std::string _DockSlot = "");
	virtual ~SToolBar() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize(const std::any& Arg) override;
	virtual void Render() override;
	virtual void Destroy() override;

private:
	bool IsEqualToolMode(EToolMode::Type Equal) const { return ToolMode[0] == Equal; }
	void SetToolMode(EToolMode::Type NewToolMode, bool bForce = true, bool bEvent = false);

	// image 
	FImageHandle ImageRectangleMarquee;
	FImageHandle ImagePencil;
	FImageHandle ImageEraser;
	FImageHandle ImageEyedropper;
	FImageHandle ImagePaintBucket;

	EToolMode::Type ToolMode[2];
};
