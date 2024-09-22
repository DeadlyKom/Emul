

#include <map>
#include <string>
#include <iostream>
#include <AppDebugger.h>

int main(int argc, char** argv)
{
	Utils::Log(Debugger::Name.c_str());

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

	return FAppFramework::Get<FAppDebugger>().Launch(Args);
}
