#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>

namespace ESubcolor
{
	enum Type : uint8_t
	{
		Ink,
		Paper,
		Bright,
		Flash,

		MAX,
		None = (uint8_t)INDEX_NONE,
	};
}

class SPalette : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SPalette;
public:
	SPalette(EFont::Type _FontName);
	virtual ~SPalette() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize() override;
	virtual void Render() override;

private:
	void Display_Colors();

	uint32_t OptionsFlags;
	uint8_t ButtonColorIndex[2];
	uint8_t ColorSelectedIndex[ESubcolor::MAX];
};
