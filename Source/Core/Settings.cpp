#include "Settings.h"
#include <json/json.hpp>

bool FSettings::Load(const std::filesystem::path& FilePath)
{
	std::error_code ec;
	if (!std::filesystem::exists(FilePath, ec))
	{
		LOG("File does not exist: {}", FilePath.string().c_str());
		return false;
	}

	std::ifstream File(FilePath, std::ios::in | std::ios::binary);
	if (!File.is_open())
	{
		LOG("Could not open the file: {}", FilePath.string().c_str());
		return false;
	}
	nlohmann::json Json;
	File >> Json;

	for (auto It = Json.begin(); It != Json.end(); ++It)
	{
		const std::string& Name = It.key();
		const auto& Item = It.value();

		if (Item.is_number_integer())
		{
			const int32_t Value = Item.get<int32_t>();
			Container[{Name, typeid(int32_t)}] = Value;
		}
		else if (Item.is_number_float())
		{
			const float Value = Item.get<float>();
			Container[{Name, typeid(float)}] = Value;
		}
		else if (Item.is_string())
		{
			const std::string Value = Item.get<std::string>();
			Container[{Name, typeid(std::string)}] = Value;
		}
		else if (Item.is_boolean())
		{
			bool Value = Item.get<bool>();
			Container[{Name, typeid(bool)}] = Value;
		}
		else if (Item.is_array())
		{
			std::vector<std::string> Values;
			for (auto& Element : Item)
			{
				if (Element.is_string())
				{
					Values.push_back(Element.get<std::string>());
				}
			}
			Container[{Name, typeid(std::vector<std::string>)}] = Values;
		}
		else if (Item.is_object())
		{
			std::map<std::string, std::string> Map;
			for (auto It = Item.begin(); It != Item.end(); ++It)
			{
				if (It.value().is_string())
				{
					Map[It.key()] = It.value().get<std::string>();
				}
			}
			Container[{Name, typeid(std::map<std::string, std::string>)}] = Map;
		}
	}

	return true;
}

bool FSettings::Save(const std::filesystem::path& FilePath)
{
	return false;
}