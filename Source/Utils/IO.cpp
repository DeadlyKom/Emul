#include "IO.h"
#include <system_error>

std::filesystem::path IO::GetUniquePath(const std::filesystem::path& Path, std::error_code& output_ec)
{
    if (!std::filesystem::exists(Path, output_ec))
    {
        return Path;
    }

    std::filesystem::path parent = Path.parent_path();
    std::filesystem::path stem = Path.stem();
    std::filesystem::path extension = Path.extension();

    int32_t Counter = 1;
    std::filesystem::path NewPath;
    do
    {
        NewPath = parent / (stem.string() + "(" + std::to_string(Counter++) + ")" + extension.string());
    } while (std::filesystem::exists(NewPath));

    return NewPath;
}

std::error_code IO::SaveBinaryData(const std::vector<uint8_t>& Data, const std::filesystem::path& FilePath)
{
    std::error_code ec;
    if (!std::filesystem::exists(FilePath.parent_path(), ec))
    {
        if (!std::filesystem::create_directories(FilePath.parent_path(), ec))
        {
            return ec;
        }
    }

    const std::filesystem::path UniqueFilePath = GetUniquePath(FilePath, ec);
    if (ec)
    {
        return ec;
    }

    std::ofstream File(UniqueFilePath, std::ios::binary);
    if (!File.is_open())
    {
        return make_error_code(std::errc::io_error);
    }

    File.write(reinterpret_cast<const char*>(Data.data()), Data.size());
    if (!File)
    {
        return make_error_code(std::errc::io_error);
    }
    return std::error_code{};
}

std::filesystem::path IO::NormalizePath(const std::filesystem::path& InOut)
{
    std::string PathString = InOut.string();
    std::replace(PathString.begin(), PathString.end(), '\\', '/');
    return std::filesystem::path(PathString);
}
