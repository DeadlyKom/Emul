#pragma once

#include <CoreMinimal.h>
#include "Viewer.h"

class SMemoryDump : public SViewerChild
{
	using Super = SViewerChild;
	using ThisClass = SMemoryDump;
public:
	SMemoryDump(EFont::Type _FontName);

	virtual void Initialize() override;
	virtual void Render() override;
};
