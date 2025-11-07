#pragma once

#include <CoreMinimal.h>

struct FSettingKey
{
	std::string Name;
	std::type_index Type;

	bool operator==(const FSettingKey& Other) const
	{
		return Name == Other.Name && Type == Other.Type;
	}
};

struct FSettingKeyHash
{
	std::size_t operator()(const FSettingKey& Key) const
	{
		//return std::hash<std::string>()(Key.Name) ^ Key.Type.hash_code();
		std::size_t h1 = std::hash<std::string>{}(Key.Name);
		std::size_t h2 = Key.Type.hash_code();
		return h1 ^ (h2 << 1);
	}
};

class FSettings
{
public:
	static FSettings& Get()
	{
		static std::shared_ptr<FSettings> Instance(new FSettings());
		return *Instance.get();
	}

	bool Load(const std::filesystem::path& FilePath);
	bool Save(const std::filesystem::path& FilePath);

	template<typename T>
	std::optional<T> GetValue(const FSettingKey& Key) const
	{
		auto It = Container.find(Key);
		if (It == Container.end())
		{
			return std::nullopt;
		}

		try
		{
			return std::any_cast<T>(It->second);
		}
		catch (const std::bad_any_cast& e)
		{
			LOG("Error: get value - ", e.what());
			return std::nullopt;
		}
	}

	template<typename T>
	std::optional<T> GetValue(const std::string& Name) const
	{
		return GetValue<T>({ Name, typeid(T) });
	}

	std::any& operator[](const FSettingKey& Key)
	{
		return Container[Key];
	}

	template<typename T>
	void SetValue(const FSettingKey& Key, const T& Value)
	{
		Container[Key] = Value;
	}

	template<typename T>
	void SetValue(const std::string& Name, const T& Value)
	{
		Container[{ Name, typeid(T) }] = Value;
	}

protected:
	FSettings() = default;
	FSettings(const FSettings&) = delete;
	FSettings& operator=(const FSettings&) = delete;

private:
	std::vector<FSettingKey> KeyOrder;
	std::unordered_map<FSettingKey, std::any, FSettingKeyHash> Container;
};