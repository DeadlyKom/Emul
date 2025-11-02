#pragma once

#include <CoreMinimal.h>

namespace IO
{
	bool SaveBinaryData(const std::vector<uint8_t>& Data, const std::filesystem::path& FilePath);
}
