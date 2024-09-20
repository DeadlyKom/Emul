#include "Name.h"

static std::list<std::string> GlobalNames;

FName::FName(const char* _Name)
{
	auto It = std::find_if(GlobalNames.begin(), GlobalNames.end(), 
		[=](const std::string& n) -> bool
		{
			return n.compare(_Name) == 0;
		});

	if (It == GlobalNames.end())
	{
		GlobalNames.push_back(_Name);
		ID = GlobalNames.size() - 1;
	}
}
