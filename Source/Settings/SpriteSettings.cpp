#include "SpriteSettings.h"


const char* FSpriteSettings::ScriptFilesTag = TEXT("ScriptFiles");

FSpriteSettings& FSpriteSettings::Get()
{
	static std::shared_ptr<FSpriteSettings> Instance(new FSpriteSettings);
	return *Instance.get();
}
