#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>

namespace EToolMode
{
	enum Type
	{
		None,
		RectangleMarquee,
		Pencil,
		Eraser,
		Eyedropper,
		PaintBucket
	};
}

class SToolBar : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SToolBar;
public:
	SToolBar(EFont::Type _FontName);
	virtual ~SToolBar() = default;

	virtual void Initialize() override;
	virtual void Render() override;
};
