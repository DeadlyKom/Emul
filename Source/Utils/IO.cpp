#include "IO.h"

bool IO::SaveBinaryData(const std::vector<uint8_t>& Data, const std::filesystem::path& FilePath)
{
    std::ofstream File(FilePath, std::ios::binary);
    if (!File.is_open())
    {
        return false;
    }

    File.write(reinterpret_cast<const char*>(Data.data()), Data.size());
    return true;
}
