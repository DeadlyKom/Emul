#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include <Utils/UI/Draw_ZXColorVideo.h>

struct FSpriteProperty
{
	using ValueType = std::variant<bool, int, float, std::string>;
	inline static constexpr const char* TypeNames[] =
	{
		"bool",
		"int",
		"float",
		"string"
	};

	FSpriteProperty(std::string _Name = "", ValueType _Variant = {})
		: Name(std::move(_Name))
		, Variant(std::move(_Variant))
	{}

	void TypeByIndex(int32_t Index)
	{
		switch (Index)
		{
		case 0: Variant = false;			break;
		case 1: Variant = int(0);			break;
		case 2: Variant = float(0.f);		break;
		case 3: Variant = std::string("");	break;
		}
	}
	int32_t IndexByType()
	{
		return (int32_t)Variant.index();
	}

	std::string_view GetTypeName() const noexcept
	{
		const size_t Index = Variant.index();
		return Index < std::size(TypeNames) ? TypeNames[Index] : "Unknown";
	}

	std::string ToString()
	{
		return std::visit([](auto&& arg) -> std::string
			{
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, bool>)
				{
					return arg ? "true" : "false";
				}
				else if constexpr (std::is_same_v<T, std::string>)
				{
					return arg;
				}
				else if constexpr (std::is_same_v<T, float>)
				{
					std::ostringstream oss;
					oss << std::fixed << std::setprecision(15) << arg;
					std::string s = oss.str(); 
					if (s.find('.') != std::string::npos)
					{
						while (!s.empty() && s.back() == '0') s.pop_back();
						if (!s.empty() && s.back() == '.') s.pop_back();
					}
					return s;
				}
				else
				{
					return std::to_string(arg);
				}
			}, Variant);
	}

	std::string Name;
	ValueType Variant;
};

struct FSpriteMetaRegion
{
	ImRect Rect;
	std::vector<FSpriteProperty> Properties;

	// internal variable
	bool bVisible = true;
	std::shared_ptr<UI::FZXColorView> ZXColorView;
};

struct FSprite
{
	uint32_t Width;
	uint32_t Height;
	std::string Name;

	// data for the editor
	uint32_t SpritePositionToImageX;
	uint32_t SpritePositionToImageY;
	std::filesystem::path SourcePathFile;

	// internal variable
	bool bSelected = false;
	float Scale = 1.0f;
	double HoverStartTime;

	uint32_t OptionsFlags;
	std::vector<FSpriteMetaRegion> Regions;
	std::shared_ptr<UI::FZXColorView> ZXColorView;
};

class SSpriteList : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SSpriteList;

public:
	SSpriteList(EFont::Type _FontName, std::string DockSlot = "");
	virtual ~SSpriteList() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize(const std::any& Arg) override;
	virtual void Render() override;
	virtual void Destroy() override;

private:
	void Input_Mouse();

	void Draw_SpriteList();
	void Draw_ExportSprites();

	void AddSprite(
		const ImRect& SpriteRect,
		const std::string& Name,
		const std::filesystem::path& SourcePathFile,
		int32_t Width, int32_t Height,
		const std::vector<uint8_t>& IndexedData,
		const std::vector<uint8_t>& InkData,
		const std::vector<uint8_t>& AttributeData,
		const std::vector<uint8_t>& MaskData);

	void ExportSprites(const std::filesystem::path& ScriptFilePath, const std::filesystem::path& ExportPath, const std::vector<std::shared_ptr<FSprite>>& SelectedSprites);

	void SendSelectedSprite() const;

	bool bNeedKeptOpened_ExportPopup;
	uint32_t ScaleVisible;
	int32_t IndexSelectedSprite;
	ImGuiID SpriteListID;
	std::filesystem::path CurrentPath;
	std::vector<std::shared_ptr<FSprite>> Sprites;

	// popup menu 'Export'
	int32_t IndexSelectedScript;
	std::vector<std::string> ScriptFileNames;
	std::map<std::string, std::string> ScriptFiles;

	// popup menu 'Edit Metadata'
	ImGuiID PopupMenu_EditMetadataID;
};
