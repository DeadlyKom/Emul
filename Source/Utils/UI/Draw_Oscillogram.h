#pragma once

#include <CoreMinimal.h>
#include "Utils/Signal/Bus.h"

namespace UI
{
	struct FOscillographStyle
	{
		ImVec2 OscillograPadding = ImVec2(1.0f, 1.0f);	// padding between widget frame and oscillogram area
	};

	struct FOscillograph
	{
		FOscillographStyle Style;
	};

	void Draw_Oscillogram(const char* TableID, FOscillograph& Oscillograph, const ImVec2& SizeArg = ImVec2(-1.0f, -1.0f), const ImVec2* _Padding = nullptr);
}
