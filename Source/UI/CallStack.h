#pragma once

#include <CoreMinimal.h>
#include "Viewer.h"

class SCallStack : public SViewerChild
{
	using Super = SViewerChild;
	using ThisClass = SCallStack;
public:
	SCallStack(EFont::Type _FontName);

	virtual void Initialize() override;
	virtual void Render() override;
};
