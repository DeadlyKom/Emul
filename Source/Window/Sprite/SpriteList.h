#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>

class SSpriteList : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SSpriteList;
public:
	SSpriteList(EFont::Type _FontName);
	virtual ~SSpriteList() = default;

	virtual void Initialize() override;
	virtual void Render() override;
};
