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
	nlohmann::ordered_json Json;
	File >> Json;
	File.close();

	Container.clear();
	KeyOrder.clear();

	for (auto It = Json.begin(); It != Json.end(); ++It)
	{
		const std::string& Name = It.key();
		const auto& Item = It.value();

		if (Item.is_number_integer())
		{
			Container[{Name, typeid(int32_t)}] = Item.get<int32_t>();
			KeyOrder.push_back({ Name, typeid(int32_t) });
		}
		else if (Item.is_number_float())
		{
			Container[{Name, typeid(float)}] = Item.get<float>();
			KeyOrder.push_back({ Name, typeid(float) });
		}
		else if (Item.is_string())
		{
			Container[{Name, typeid(std::string)}] = Item.get<std::string>();
			KeyOrder.push_back({ Name, typeid(std::string) });
		}
		else if (Item.is_boolean())
		{
			Container[{Name, typeid(bool)}] = Item.get<bool>();
			KeyOrder.push_back({ Name, typeid(bool) });
		}
		else if (Item.is_array())
		{
			std::vector<std::string> Values;
			for (auto& Element : Item)
			{
				if (Element.is_string())
					Values.push_back(Element.get<std::string>());
			}
			Container[{Name, typeid(std::vector<std::string>)}] = Values;
			KeyOrder.push_back({ Name, typeid(std::vector<std::string>) });
		}
		else if (Item.is_object())
		{
			std::map<std::string, std::string> Map;
			for (auto SubIt = Item.begin(); SubIt != Item.end(); ++SubIt)
			{
				if (SubIt.value().is_string())
				{
					Map[SubIt.key()] = SubIt.value().get<std::string>();
				}
			}
			Container[{Name, typeid(std::map<std::string, std::string>)}] = Map;
			KeyOrder.push_back({ Name, typeid(std::map<std::string, std::string>) });
		}
	}
	return true;
}

bool FSettings::Save(const std::filesystem::path& FilePath)
{
	std::error_code ec;
	if (!std::filesystem::exists(FilePath.parent_path(), ec))
	{
		if (!std::filesystem::create_directories(FilePath.parent_path(), ec))
		{
			LOG("Could not create directory: {}. Error: {}", FilePath.parent_path().string(), ec.message());
			return false;
		}
	}

	nlohmann::ordered_json Json;
	for (const auto& Key : KeyOrder)
	{
		auto It = Container.find(Key);
		if (It == Container.end())
		{
			continue;
		}

		const auto& Value = It->second;

		if (Key.Type == typeid(int32_t))
		{
			Json[Key.Name] = std::any_cast<int32_t>(Value);
		}
		else if (Key.Type == typeid(float))
		{
			Json[Key.Name] = std::any_cast<float>(Value);
		}
		else if (Key.Type == typeid(std::string))
		{
			Json[Key.Name] = std::any_cast<std::string>(Value);
		}
		else if (Key.Type == typeid(bool))
		{
			Json[Key.Name] = std::any_cast<bool>(Value);
		}
		else if (Key.Type == typeid(std::vector<std::string>))
		{
			Json[Key.Name] = std::any_cast<std::vector<std::string>>(Value);
		}
		else if (Key.Type == typeid(std::map<std::string, std::string>))
		{
			nlohmann::ordered_json SubJson;
			const auto& MapValue = std::any_cast<std::map<std::string, std::string>>(Value);
			for (const auto& [SubKey, SubVal] : MapValue)
			{
				SubJson[SubKey] = SubVal;
			}
			Json[Key.Name] = SubJson;
		}
	}

	std::ofstream File(FilePath, std::ios::out | std::ios::trunc | std::ios::binary);
	if (!File.is_open())
	{
		LOG("Could not open file for writing: {}", FilePath.string().c_str());
		return false;
	}

	File << std::setw(4) << Json; // красиво отформатировать JSON (4 пробела)
	File.close();

	if (ec)
	{
		LOG("Error saving file {}: {}", FilePath.string(), ec.message());
		return false;
	}

	LOG("Settings saved successfully: {}", FilePath.string());
	return true;
}