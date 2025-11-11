#pragma once

#include <CoreMinimal.h>

namespace IO
{
	std::filesystem::path GetUniquePath(const std::filesystem::path& Path, std::error_code& output_ec);
	std::error_code SaveBinaryData(const std::vector<uint8_t>& Data, const std::filesystem::path& FilePath, bool bUniqueFilename = true);
	std::error_code LoadBinaryData(std::vector<uint8_t>& OutputData, const std::filesystem::path& FilePath);
	std::filesystem::path NormalizePath(const std::filesystem::path& InOut);
}
