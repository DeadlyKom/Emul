#pragma once

#include <CoreMinimal.h>
#include "Viewer.h"
#include "Utils/UI/Draw_Oscillogram.h"

class SOscillograph : public SViewerChild
{
	using Super = SViewerChild;
	using ThisClass = SOscillograph;
public:
	SOscillograph(EFont::Type _FontName);
	virtual ~SOscillograph() = default;

	virtual void Initialize(const std::any& Arg) override;
	virtual void Render() override;

private:
	UI::FOscillograph Oscillograph;
};
