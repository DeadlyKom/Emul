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

	virtual void Initialize() override;
	virtual void Render() override;
};
