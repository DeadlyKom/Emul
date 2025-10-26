#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>

class SPalette : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SPalette;
public:
	SPalette(EFont::Type _FontName);
	virtual ~SPalette() = default;

	virtual void Initialize() override;
	virtual void Render() override;

private:
	void Display_Colors();

	int32_t ColorSelectedIndex;
};
