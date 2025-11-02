#pragma once

#include <CoreMinimal.h>
#include <Core/Settings.h>

class FSpriteSettings : public FSettings
{
	using Super = FSettings;
public:
	static FSpriteSettings& Get(); 

	static const char* ScriptFilesTag;
};
