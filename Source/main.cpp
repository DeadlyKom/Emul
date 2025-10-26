
#include <map>
#include <string>
#include <iostream>
#include <AppMain.h>
#include <AppSprite.h>
#include <AppDebugger.h>

int main(int argc, char** argv)
{
	std::map<std::string, std::string> Args;
	for (int i = 0; i < argc; ++i)
	{
		if (argv[i][0] == '-')
		{
			std::string Value;
			const std::string ParamName = argv[i] + 1;
			if (i + 1 < argc && argv[i + 1][0] != '-')
			{
				i++;
				Value = argv[i];
			}
			Args.emplace(ParamName, Value);
		}
	}

	EApplication::Type Application = EApplication::None;
	{
		for (const auto& [Key, Value] : Args)
		{
			if (!Key.compare("debugger"))
			{
				Application = EApplication::Debugger;
				break;
			}
			else if (!Key.compare("sprite"))
			{
				Application = EApplication::Sprite;
				break;
			}
		}

		if (Application == EApplication::None)
		{
			Application = (EApplication::Type)FAppFramework::Get<FAppMain>().Launch(Args);
		}
	}

	switch (Application)
	{
	case EApplication::None:
	default:
		return 0;

	case EApplication::Debugger:
		return FAppFramework::Get<FAppDebugger>().Launch(Args);

	case EApplication::Sprite:
		return FAppFramework::Get<FAppSprite>().Launch(Args);
	}
}
