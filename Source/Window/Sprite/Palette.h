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

		All,
		None = (uint8_t)INDEX_NONE,
	};
}

class SPalette : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SPalette;
public:
	SPalette(EFont::Type _FontName, std::string _DockSlot = "");
	virtual ~SPalette() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Render() override;
	virtual void Destroy() override;

private:
	void Display_Colors();

	uint32_t OptionsFlags;
	uint8_t ButtonColor[2];
	uint8_t Subcolor[ESubcolor::MAX];
};
