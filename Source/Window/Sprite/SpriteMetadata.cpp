#include "SpriteMetadata.h"
#include <Window/Sprite/Events.h>
#include "SpriteList.h"

#include <Utils/Array.h>
#include <Utils/Char.h>
#include <Utils/IO.h>
#include <Utils/UI/Draw.h>
#include <Core/AppFramework.h>
#include <Settings/SpriteSettings.h>
#include <json/json.hpp>

namespace
{
	static const wchar_t* ThisWindowName = L"Sprite Metadata";
}

SSpriteMetadata::SSpriteMetadata(EFont::Type _FontName, std::string _DockSlot /*= ""*/)
	: Super(FWindowInitializer()
		.SetOpen(false)
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetDockSlot(_DockSlot)
		.SetIncludeInWindows(false))
	, HeverTooltip(0.0f)
	, IndexSelectedRegion(INDEX_NONE)
	, IndexSelectedProperty(INDEX_NONE)
{}

void SSpriteMetadata::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_SelectedSprite>(
		[this](const FEvent_SelectedSprite& Event)
		{
			if (Event.Tag == FEventTag::SelectedSpritesChangedTag)
			{
				if (SelectedSprite != Event.Sprite)
				{
					IndexSelectedRegion = INDEX_NONE;
					IndexSelectedProperty = INDEX_NONE;
					PropertySelectionAnchor = INDEX_NONE;
					SelectedPropertyIndices.clear();
					EditingProperty.clear();
				}
				SelectedSprite = Event.Sprite;
			}
		});
}

void SSpriteMetadata::Initialize(const std::vector<std::any>& Args)
{
	Super::Initialize(Args);

	constexpr const char* DefaultPresetsPath = "Saved/Properties";
	FSpriteSettings& Settings = FSpriteSettings::Get();
	const auto ConfiguredPath = Settings.GetValue<std::string>(FSpriteSettings::MetadataPresetsPathTag);
	const bool bMigrateOldPath = ConfiguredPath.has_value() && *ConfiguredPath == "Properties";
	if (ConfiguredPath.has_value() && !ConfiguredPath->empty() && !bMigrateOldPath)
	{
		MetadataPresetsPath = Utils::Utf8ToUtf16(*ConfiguredPath);
	}
	else
	{
		MetadataPresetsPath = DefaultPresetsPath;
		Settings.SetValue<std::string>(FSpriteSettings::MetadataPresetsPathTag, DefaultPresetsPath);
		const std::filesystem::path SettingsPath = FAppFramework::GetPath(EPathType::Config) / FAppFramework::GetFilename(EFilenameType::Config);
		Settings.Save(IO::NormalizePath(SettingsPath));
	}

	if (MetadataPresetsPath.is_relative())
	{
		std::wstring ExecutablePath(32768, L'\0');
		const DWORD PathLength = GetModuleFileNameW(nullptr, ExecutablePath.data(), static_cast<DWORD>(ExecutablePath.size()));
		if (PathLength > 0 && PathLength < ExecutablePath.size())
		{
			ExecutablePath.resize(PathLength);
			MetadataPresetsPath = std::filesystem::path(ExecutablePath).parent_path() / MetadataPresetsPath;
		}
		else
		{
			MetadataPresetsPath = std::filesystem::current_path() / MetadataPresetsPath;
		}
	}
	MetadataPresetsPath = IO::NormalizePath(MetadataPresetsPath);
	LoadMetadataPresets();
}

void SSpriteMetadata::LoadMetadataPresets()
{
	MetadataPresets.clear();
	PresetError.clear();
	std::error_code Error;
	if (!std::filesystem::exists(MetadataPresetsPath, Error))
	{
		std::filesystem::create_directories(MetadataPresetsPath, Error);
	}
	if (Error)
	{
		PresetError = std::format("Cannot open preset directory: {}", Error.message());
		return;
	}

	for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(MetadataPresetsPath, Error))
	{
		if (Error || !Entry.is_regular_file() || Entry.path().extension() != L".json")
		{
			continue;
		}

		try
		{
			std::ifstream File(Entry.path(), std::ios::binary);
			nlohmann::ordered_json Json;
			File >> Json;
			if (!Json.is_object() || !Json.contains("Name") || !Json.contains("Properties") || !Json["Properties"].is_array())
			{
				continue;
			}

			FMetadataPreset Preset;
			Preset.Name = Json["Name"].get<std::string>();
			Preset.FilePath = Entry.path();
			for (const nlohmann::ordered_json& PropertyJson : Json["Properties"])
			{
				if (!PropertyJson.contains("Name") || !PropertyJson.contains("Type"))
				{
					continue;
				}
				FSpriteProperty Property(PropertyJson["Name"].get<std::string>());
				const std::string Type = PropertyJson["Type"].get<std::string>();
				const auto TypeIt = std::find_if(std::begin(FSpriteProperty::TypeNames), std::end(FSpriteProperty::TypeNames),
					[&Type](const char* Name) { return Type == Name; });
				if (TypeIt == std::end(FSpriteProperty::TypeNames))
				{
					continue;
				}
				const int32_t TypeIndex = static_cast<int32_t>(std::distance(std::begin(FSpriteProperty::TypeNames), TypeIt));
				Property.TypeByIndex(TypeIndex);
				if (PropertyJson.contains("Value"))
				{
					switch (TypeIndex)
					{
					case 0: Property.Variant = PropertyJson["Value"].get<bool>(); break;
					case 1: Property.Variant = PropertyJson["Value"].get<int>(); break;
					case 2: Property.Variant = PropertyJson["Value"].get<float>(); break;
					case 3: Property.Variant = PropertyJson["Value"].get<std::string>(); break;
					}
				}
				Preset.Properties.push_back(std::move(Property));
			}
			if (!Preset.Name.empty() && !Preset.Properties.empty())
			{
				MetadataPresets.push_back(std::move(Preset));
			}
		}
		catch (const std::exception& Exception)
		{
			LOG_ERROR("[{}]\t Failed to load metadata preset '{}': {}", (__FUNCTION__), Entry.path().string(), Exception.what());
		}
	}

	std::sort(MetadataPresets.begin(), MetadataPresets.end(),
		[](const FMetadataPreset& Left, const FMetadataPreset& Right) { return Left.Name < Right.Name; });
}

bool SSpriteMetadata::SaveMetadataPreset()
{
	PresetError.clear();
	const std::string PresetName = NewPresetName;
	if (PresetName.empty())
	{
		PresetError = "Preset name is required.";
		return false;
	}
	if (NewPresetProperties.empty() || std::any_of(NewPresetProperties.begin(), NewPresetProperties.end(),
		[](const FSpriteProperty& Property) { return Property.Name.empty(); }))
	{
		PresetError = "Add at least one named property.";
		return false;
	}

	nlohmann::ordered_json Json;
	Json["Name"] = PresetName;
	Json["Properties"] = nlohmann::ordered_json::array();
	for (const FSpriteProperty& Property : NewPresetProperties)
	{
		nlohmann::ordered_json PropertyJson;
		PropertyJson["Name"] = Property.Name;
		PropertyJson["Type"] = std::string(Property.GetTypeName());
		std::visit([&PropertyJson](const auto& Value) { PropertyJson["Value"] = Value; }, Property.Variant);
		Json["Properties"].push_back(std::move(PropertyJson));
	}

	std::string FileName = PresetName;
	for (char& Character : FileName)
	{
		if (Character == '<' || Character == '>' || Character == ':' || Character == '"' || Character == '/' ||
			Character == '\\' || Character == '|' || Character == '?' || Character == '*')
		{
			Character = '_';
		}
	}
	const std::filesystem::path FilePath = MetadataPresetsPath / (Utils::Utf8ToUtf16(FileName) + L".json");
	std::error_code Error;
	std::filesystem::create_directories(MetadataPresetsPath, Error);
	if (Error)
	{
		PresetError = Error.message();
		return false;
	}

	std::ofstream File(FilePath, std::ios::binary | std::ios::trunc);
	if (!File.is_open())
	{
		PresetError = "Cannot write preset file.";
		return false;
	}
	File << Json.dump(4);
	File.close();
	LoadMetadataPresets();
	return true;
}

void SSpriteMetadata::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen, ImGuiWindowFlags_NoResize);
	{
		if (SelectedSprite)
		{
			Draw_Metadata();
		}
		ImGui::End();
	}

	if (!IsOpen())
	{
		EditingProperty.clear();
	}
}

void SSpriteMetadata::Tick(float DeltaTime)
{
	HeverTooltip += DeltaTime;
}

void SSpriteMetadata::Destroy()
{
	
	UnsubscribeAll();
}

void SSpriteMetadata::Draw_Metadata()
{
	if (ImGui::IsWindowAppearing())
	{
		ImGui::SetWindowSize(ImVec2(850.0f, 420.0f));
		ImGui::SetWindowCollapsed(false);
		IndexSelectedRegion = INDEX_NONE;
		IndexSelectedProperty = INDEX_NONE;
		PropertySelectionAnchor = INDEX_NONE;
		SelectedPropertyIndices.clear();
	}

	const bool bEditingAnyProperty = std::any_of(EditingProperty.begin(), EditingProperty.end(),
		[](const auto& Pair) { return Pair.second; });
	if (!bEditingAnyProperty && !ImGui::IsAnyItemActive() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C))
		{
			CopySelectedProperties(false);
		}
		else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X))
		{
			CopySelectedProperties(true);
		}
		else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
		{
			PasteProperties();
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_Delete))
		{
			DeleteSelectedProperties();
		}
	}

	ImVec2 RegionSize, SpriteSize, PropertySize, ButtonPosition;
	const float TextWidth = ImGui::CalcTextSize("A").x;
	const float TextHeight = ImGui::GetTextLineHeightWithSpacing();

	const ImVec2 Size = ImGui::GetWindowSize();
	ImGuiStyle& Style = ImGui::GetStyle();

	const float HeightButton = TextWidth * 5.0f;

	RegionSize.x = Size.x * 0.3f;
	RegionSize.y = Size.y * 1.0f - Style.WindowPadding.y * 3 - HeightButton - TextWidth;

	SpriteSize.x = Size.x * 0.3f - Style.WindowPadding.x * 3;
	SpriteSize.y = SpriteSize.x;

	PropertySize.x = Size.x - RegionSize.x - SpriteSize.x - Style.WindowPadding.x * 5;
	PropertySize.y = Size.y * 1.0f - Style.WindowPadding.y * 3 - HeightButton - TextWidth;

	ButtonPosition.x = RegionSize.x - ImGui::CalcTextSize(" + ").x - Style.WindowPadding.y * 3;
	ButtonPosition.y = Size.y * 1.0f - Style.WindowPadding.y * 3 - TextWidth;

	Draw_Regions(RegionSize);
	ImGui::SameLine();
	Draw_Sprite(SpriteSize);
	ImGui::SameLine();
	Draw_Property(PropertySize);	
	ImGui::SameLine();
	Draw_Buttons(ButtonPosition, HeightButton);
}

void SSpriteMetadata::Draw_Regions(const ImVec2& Size)
{
	if (ImGui::BeginChild("Meta", Size, true, ImGuiWindowFlags_HorizontalScrollbar))
	{
		for (int32_t RegionIndex = 0; RegionIndex < SelectedSprite->Regions.size(); ++RegionIndex)
		{
			const bool bIsSelectedRegion = RegionIndex == IndexSelectedRegion;
			FSpriteMetaRegion& SpriteMetaRegion = SelectedSprite->Regions[RegionIndex];
			const std::string HeaderName = SpriteMetaRegion.bHasRegionRect
				? std::format(" {} Region #{} {}##Region{}", bIsSelectedRegion ? ">" : "", RegionIndex, bIsSelectedRegion ? "<" : "", RegionIndex)
				: std::format(" {} Entire sprite {}##Region{}", bIsSelectedRegion ? ">" : "", bIsSelectedRegion ? "<" : "", RegionIndex);

			const bool bHeaderOpen = ImGui::CollapsingHeader(
				HeaderName.c_str(),
				&SpriteMetaRegion.bVisible,
				ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_OpenOnDoubleClick |
				ImGuiTreeNodeFlags_SpanFullWidth |
				(bIsSelectedRegion ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None));

			if (ImGui::IsItemClicked())
			{
				IndexSelectedRegion = RegionIndex;
				IndexSelectedProperty = INDEX_NONE;
				PropertySelectionAnchor = INDEX_NONE;
				SelectedPropertyIndices.clear();
			}

			if (ImGui::IsItemHovered() && SpriteMetaRegion.bHasRegionRect)
			{
				ImGui::BeginTooltip();
				ImGui::TextUnformatted("Region info:");
				ImGui::Spacing();
				ImGui::TextUnformatted(std::format(" - size: {} x {}", SpriteMetaRegion.Rect.GetSize().x, SpriteMetaRegion.Rect.GetSize().y).c_str());
				ImGui::EndTooltip();
			}

			if (bHeaderOpen)
			{
				for (int32_t PropertyIndex = 0; PropertyIndex < SpriteMetaRegion.Properties.size(); ++PropertyIndex)
				{
					const std::string EditingPropertName = std::format("{}_PropertyName{}", RegionIndex, PropertyIndex);

					FSpriteProperty& Property = SpriteMetaRegion.Properties[PropertyIndex];
					const bool bIsSelectedProperty = bIsSelectedRegion && SelectedPropertyIndices.contains(PropertyIndex);

					// Enable editing by pressing F2
					if (bIsSelectedRegion && PropertyIndex == IndexSelectedProperty && ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_F2))
					{
						EditingProperty[EditingPropertName] = true;
					}

					// Enable double-click editing
					if (EditingProperty[EditingPropertName])
					{
						ImGui::SetKeyboardFocusHere();

						char InputBuffer[128];
						std::strncpy(InputBuffer, Property.Name.c_str(), sizeof(InputBuffer));
						InputBuffer[sizeof(InputBuffer) - 1] = '\0';

						if (ImGui::InputText(std::format("##PropertyName{}", PropertyIndex).c_str(), InputBuffer, sizeof(InputBuffer),
							ImGuiInputTextFlags_AutoSelectAll))
						{
							Property.Name = InputBuffer;
						}

						// Keep the edited name and finish editing when the field loses focus.
						if (ImGui::IsItemDeactivated() || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape))
						{
							EditingProperty[EditingPropertName] = false;
						}
						ImGui::SetItemDefaultFocus(); // the cursor is immediately in InputText
					}
					else
					{
						const std::string PropertyName = std::format("{}##{}_PropertyName{}", Property.Name.c_str(), RegionIndex, PropertyIndex);
						if (ImGui::Selectable(PropertyName.c_str(), bIsSelectedProperty))
						{
							const ImGuiIO& IO = ImGui::GetIO();
							const bool bChangedRegion = IndexSelectedRegion != RegionIndex;
							if (bChangedRegion || (!IO.KeyCtrl && !IO.KeyShift))
							{
								SelectedPropertyIndices.clear();
							}
							if (bChangedRegion)
							{
								PropertySelectionAnchor = INDEX_NONE;
							}
							IndexSelectedRegion = RegionIndex;
							if (IO.KeyShift && PropertySelectionAnchor >= 0)
							{
								const int32_t First = min(PropertySelectionAnchor, PropertyIndex);
								const int32_t Last = max(PropertySelectionAnchor, PropertyIndex);
								for (int32_t SelectionIndex = First; SelectionIndex <= Last; ++SelectionIndex)
								{
									SelectedPropertyIndices.insert(SelectionIndex);
								}
							}
							else if (IO.KeyCtrl && SelectedPropertyIndices.contains(PropertyIndex))
							{
								SelectedPropertyIndices.erase(PropertyIndex);
							}
							else
							{
								SelectedPropertyIndices.insert(PropertyIndex);
								PropertySelectionAnchor = PropertyIndex;
							}
							IndexSelectedProperty = SelectedPropertyIndices.contains(PropertyIndex)
								? PropertyIndex
								: (SelectedPropertyIndices.empty() ? INDEX_NONE : *SelectedPropertyIndices.rbegin());
						}

						if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
						{
							EditingProperty[EditingPropertName] = true;
						}
					}
				}
			}

			if (!SpriteMetaRegion.bVisible)
			{
				if (RemoveAtSwap(SelectedSprite->Regions, RegionIndex))
				{
					IndexSelectedRegion = INDEX_NONE;
					IndexSelectedProperty = INDEX_NONE;
					PropertySelectionAnchor = INDEX_NONE;
					SelectedPropertyIndices.clear();
					--RegionIndex;
				}
				continue;
			}
		}
	}
	ImGui::EndChild();
}

void SSpriteMetadata::Draw_Sprite(const ImVec2& Size)
{
	UI::FButtonZXColorSpriteSettings::FLayer Layers = {};
	if (IndexSelectedRegion != INDEX_NONE &&
		SelectedSprite->Regions.size() > IndexSelectedRegion &&
		SelectedSprite->Regions[IndexSelectedRegion].bHasRegionRect)
	{
		FSpriteMetaRegion& SpriteMetaRegion = SelectedSprite->Regions[IndexSelectedRegion];
		Layers = { SpriteMetaRegion.ZXColorView };
	}
	UI::FButtonZXColorSpriteSettings Settings
	{
		.bHovered = false,
		.bVisibleAdvencedTooltip = false,
		.Layers = Layers,
	};
	UI::Draw_ButtonZXColorSprite("Sprite", SelectedSprite, Size, Settings);
}

void SSpriteMetadata::Draw_Property(const ImVec2& Size)
{
	if (IndexSelectedRegion == INDEX_NONE ||
		IndexSelectedRegion >= SelectedSprite->Regions.size() ||
		IndexSelectedProperty == INDEX_NONE ||
		IndexSelectedProperty >= SelectedSprite->Regions[IndexSelectedRegion].Properties.size())
	{
		return;
	}

	FSpriteMetaRegion& Region = SelectedSprite->Regions[IndexSelectedRegion];
	FSpriteProperty& Property = Region.Properties[IndexSelectedProperty];

	if (ImGui::BeginChild("Property", Size, true, ImGuiWindowFlags_HorizontalScrollbar))
	{	
		ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 0.5f));
		int32_t IndexSelectedMetaType = Property.IndexByType();
		ImGui::SeparatorText("Type : ");
		ImGui::PushItemWidth(100.0f);
		if (ImGui::Combo("###ItemType", &IndexSelectedMetaType, FSpriteProperty::TypeNames, IM_ARRAYSIZE(FSpriteProperty::TypeNames), IM_ARRAYSIZE(FSpriteProperty::TypeNames)))
		{
			Property.TypeByIndex(IndexSelectedMetaType);
		}
		ImGui::PopItemWidth();

		ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 1.0f));
		ImGui::SeparatorText("Value : ");
		const std::string ValueID = std::format("##Value{}_{}", IndexSelectedRegion, IndexSelectedProperty);
		std::visit([&ValueID](auto& Value)
			{
				using TValue = std::decay_t<decltype(Value)>;
				if constexpr (std::is_same_v<TValue, bool>)
				{
					ImGui::Checkbox(ValueID.c_str(), &Value);
				}
				else if constexpr (std::is_same_v<TValue, int>)
				{
					ImGui::InputInt(ValueID.c_str(), &Value);
				}
				else if constexpr (std::is_same_v<TValue, float>)
				{
					ImGui::InputFloat(ValueID.c_str(), &Value);
				}
				else if constexpr (std::is_same_v<TValue, std::string>)
				{
					char ValueBuffer[256];
					std::strncpy(ValueBuffer, Value.c_str(), sizeof(ValueBuffer));
					ValueBuffer[sizeof(ValueBuffer) - 1] = '\0';
					if (ImGui::InputText(ValueID.c_str(), ValueBuffer, sizeof(ValueBuffer)))
					{
						Value = ValueBuffer;
					}
				}
			}, Property.Variant);
	}
	ImGui::EndChild();
}

void SSpriteMetadata::Draw_Buttons(const ImVec2& Position, float HeightButton)
{
	ImGui::SetCursorPos(Position);
	if (ImGui::ButtonEx("+", ImVec2(HeightButton * 0.5f, HeightButton * 0.5f)))
	{
		ImGui::OpenPopup("Add metadata##AddMetadata");
	}

	bool bHovered = false;
	bHovered |= ImGui::IsItemHovered();
	if (ImGui::IsItemHovered() && HeverTooltip > 0.5f)
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("add property");
		ImGui::EndTooltip();
	}

	ImGui::SetCursorPos({ Position.x + HeightButton * 0.8f, Position.y});
	const bool bCanRemoveProperty =
		IndexSelectedRegion >= 0 &&
		IndexSelectedRegion < static_cast<int32_t>(SelectedSprite->Regions.size()) &&
		((!SelectedPropertyIndices.empty()) ||
			(IndexSelectedProperty != INDEX_NONE && IndexSelectedProperty < SelectedSprite->Regions[IndexSelectedRegion].Properties.size()));
	ImGui::BeginDisabled(!bCanRemoveProperty);
	if (ImGui::ButtonEx("-", ImVec2(HeightButton * 0.5f, HeightButton * 0.5f)))
	{
		DeleteSelectedProperties();
	}
	bHovered |= ImGui::IsItemHovered();
	if (ImGui::IsItemHovered() && HeverTooltip > 0.5f)
	{
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("remove property");
		ImGui::EndTooltip();
	}
	ImGui::EndDisabled();

	if (!bHovered)
	{
		HeverTooltip = 0.0f;
	}

	Draw_AddPropertyPopup();
}

void SSpriteMetadata::ApplyProperties(const std::vector<FSpriteProperty>& Properties)
{
	if (!SelectedSprite || Properties.empty())
	{
		return;
	}

	auto AddUnique = [&Properties](FSpriteMetaRegion& Region)
		{
			for (const FSpriteProperty& Property : Properties)
			{
				const bool bExists = std::any_of(Region.Properties.begin(), Region.Properties.end(),
					[&Property](const FSpriteProperty& Existing) { return Existing.Name == Property.Name; });
				if (!bExists)
				{
					Region.Properties.push_back(Property);
				}
			}
		};
	auto AddToEntireSprite = [&AddUnique](const std::shared_ptr<FSprite>& Sprite)
		{
			if (!Sprite)
			{
				return;
			}
			auto It = std::find_if(Sprite->Regions.begin(), Sprite->Regions.end(),
				[](const FSpriteMetaRegion& Region) { return !Region.bHasRegionRect; });
			if (It == Sprite->Regions.end())
			{
				Sprite->Regions.emplace_back(FSpriteMetaRegion{ .bHasRegionRect = false });
				It = std::prev(Sprite->Regions.end());
			}
			AddUnique(*It);
		};

	if (MetadataApplyTarget == 0)
	{
		if (IndexSelectedRegion == INDEX_NONE || IndexSelectedRegion >= static_cast<int32_t>(SelectedSprite->Regions.size()))
		{
			AddToEntireSprite(SelectedSprite);
			auto It = std::find_if(SelectedSprite->Regions.begin(), SelectedSprite->Regions.end(),
				[](const FSpriteMetaRegion& Region) { return !Region.bHasRegionRect; });
			IndexSelectedRegion = static_cast<int32_t>(std::distance(SelectedSprite->Regions.begin(), It));
		}
		else
		{
			AddUnique(SelectedSprite->Regions[IndexSelectedRegion]);
		}
		return;
	}

	bool bAppliedToSelected = false;
	FEvent_RequestSprites Event;
	Event.Callback = [&](const std::vector<std::shared_ptr<FSprite>>& Sprites)
		{
			for (const std::shared_ptr<FSprite>& Sprite : Sprites)
			{
				if (MetadataApplyTarget == 1 && !Sprite->bSelected)
				{
					continue;
				}
				AddToEntireSprite(Sprite);
				bAppliedToSelected = true;
			}
		};
	SendEvent(Event);
	if (MetadataApplyTarget == 1 && !bAppliedToSelected)
	{
		AddToEntireSprite(SelectedSprite);
	}
}

void SSpriteMetadata::AddCustomProperty()
{
	if (!SelectedSprite)
	{
		return;
	}
	if (IndexSelectedRegion < 0 || IndexSelectedRegion >= static_cast<int32_t>(SelectedSprite->Regions.size()))
	{
		auto It = std::find_if(SelectedSprite->Regions.begin(), SelectedSprite->Regions.end(),
			[](const FSpriteMetaRegion& Region) { return !Region.bHasRegionRect; });
		if (It == SelectedSprite->Regions.end())
		{
			SelectedSprite->Regions.emplace_back(FSpriteMetaRegion{ .bHasRegionRect = false });
			It = std::prev(SelectedSprite->Regions.end());
		}
		IndexSelectedRegion = static_cast<int32_t>(std::distance(SelectedSprite->Regions.begin(), It));
	}
	if (IndexSelectedRegion < 0 || IndexSelectedRegion >= static_cast<int32_t>(SelectedSprite->Regions.size()))
	{
		return;
	}

	FSpriteMetaRegion& Region = SelectedSprite->Regions[IndexSelectedRegion];
	Region.Properties.emplace_back(std::format("Property #{}", Region.Properties.size()), false);
	IndexSelectedProperty = static_cast<int32_t>(Region.Properties.size() - 1);
	PropertySelectionAnchor = IndexSelectedProperty;
	SelectedPropertyIndices = { IndexSelectedProperty };
	EditingProperty[std::format("{}_PropertyName{}", IndexSelectedRegion, IndexSelectedProperty)] = true;
}

void SSpriteMetadata::CopySelectedProperties(bool bCut)
{
	if (!SelectedSprite || IndexSelectedRegion < 0 || IndexSelectedRegion >= static_cast<int32_t>(SelectedSprite->Regions.size()))
	{
		return;
	}

	const std::vector<FSpriteProperty>& Properties = SelectedSprite->Regions[IndexSelectedRegion].Properties;
	if (SelectedPropertyIndices.empty() && IndexSelectedProperty >= 0)
	{
		SelectedPropertyIndices.insert(IndexSelectedProperty);
	}
	CopiedProperties.clear();
	for (int32_t Index : SelectedPropertyIndices)
	{
		if (Index >= 0 && Index < static_cast<int32_t>(Properties.size()))
		{
			CopiedProperties.push_back(Properties[Index]);
		}
	}
	if (bCut && !CopiedProperties.empty())
	{
		DeleteSelectedProperties();
	}
}

void SSpriteMetadata::PasteProperties()
{
	if (!SelectedSprite || CopiedProperties.empty())
	{
		return;
	}
	if (IndexSelectedRegion < 0 || IndexSelectedRegion >= static_cast<int32_t>(SelectedSprite->Regions.size()))
	{
		auto It = std::find_if(SelectedSprite->Regions.begin(), SelectedSprite->Regions.end(),
			[](const FSpriteMetaRegion& Region) { return !Region.bHasRegionRect; });
		if (It == SelectedSprite->Regions.end())
		{
			SelectedSprite->Regions.emplace_back(FSpriteMetaRegion{ .bHasRegionRect = false });
			It = std::prev(SelectedSprite->Regions.end());
		}
		IndexSelectedRegion = static_cast<int32_t>(std::distance(SelectedSprite->Regions.begin(), It));
	}

	std::vector<FSpriteProperty>& Properties = SelectedSprite->Regions[IndexSelectedRegion].Properties;
	SelectedPropertyIndices.clear();
	for (const FSpriteProperty& CopiedProperty : CopiedProperties)
	{
		const bool bExists = std::any_of(Properties.begin(), Properties.end(),
			[&CopiedProperty](const FSpriteProperty& Property) { return Property.Name == CopiedProperty.Name; });
		if (bExists)
		{
			continue;
		}
		Properties.push_back(CopiedProperty);
		SelectedPropertyIndices.insert(static_cast<int32_t>(Properties.size() - 1));
	}
	IndexSelectedProperty = SelectedPropertyIndices.empty() ? INDEX_NONE : *SelectedPropertyIndices.rbegin();
	PropertySelectionAnchor = IndexSelectedProperty;
}

void SSpriteMetadata::DeleteSelectedProperties()
{
	if (!SelectedSprite || IndexSelectedRegion < 0 || IndexSelectedRegion >= static_cast<int32_t>(SelectedSprite->Regions.size()))
	{
		return;
	}

	std::vector<FSpriteProperty>& Properties = SelectedSprite->Regions[IndexSelectedRegion].Properties;
	if (SelectedPropertyIndices.empty() && IndexSelectedProperty >= 0)
	{
		SelectedPropertyIndices.insert(IndexSelectedProperty);
	}
	for (auto It = SelectedPropertyIndices.rbegin(); It != SelectedPropertyIndices.rend(); ++It)
	{
		const int32_t Index = *It;
		if (Index >= 0 && Index < static_cast<int32_t>(Properties.size()))
		{
			Properties.erase(Properties.begin() + Index);
		}
	}
	SelectedPropertyIndices.clear();
	PropertySelectionAnchor = INDEX_NONE;
	IndexSelectedProperty = INDEX_NONE;
	EditingProperty.clear();
}

void SSpriteMetadata::Draw_AddPropertyPopup()
{
	Draw_CreatePresetPopup();
	if (!ImGui::BeginPopup("Add metadata##AddMetadata"))
	{
		return;
	}

	static constexpr const char* ApplyTargets[] = { "Current region", "Selected sprites", "All sprites" };
	ImGui::SetNextItemWidth(180.0f);
	ImGui::Combo("Apply to", &MetadataApplyTarget, ApplyTargets, IM_ARRAYSIZE(ApplyTargets));
	if (MetadataApplyTarget != 0)
	{
		ImGui::TextDisabled("Batch fields are added to Entire sprite.");
	}

	auto AddAndClose = [this](std::vector<FSpriteProperty> Properties)
		{
			ApplyProperties(Properties);
			ImGui::CloseCurrentPopup();
		};

	ImGui::SeparatorText("Presets");
	if (MetadataPresets.empty())
	{
		ImGui::TextDisabled("No presets in:");
		ImGui::TextDisabled("%s", MetadataPresetsPath.string().c_str());
	}
	for (const FMetadataPreset& Preset : MetadataPresets)
	{
		const std::string PresetLabel = std::format("{}##{}", Preset.Name, Preset.FilePath.string());
		if (ImGui::MenuItem(PresetLabel.c_str()))
		{
			AddAndClose(Preset.Properties);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(Preset.Name.c_str());
			ImGui::Separator();
			for (const FSpriteProperty& Property : Preset.Properties)
			{
				ImGui::BulletText("%s : %s", Property.Name.c_str(), Property.GetTypeName().data());
			}
			ImGui::EndTooltip();
		}
	}

	ImGui::Separator();
	if (ImGui::MenuItem("New preset..."))
	{
		NewPresetName[0] = '\0';
		NewPresetProperties = { FSpriteProperty("Property", false) };
		PresetError.clear();
		bOpenCreatePresetPopup = true;
		ImGui::CloseCurrentPopup();
	}
	if (ImGui::MenuItem("Reload presets"))
	{
		LoadMetadataPresets();
	}
	ImGui::Separator();
	if (ImGui::MenuItem("Custom...", "current region"))
	{
		AddCustomProperty();
		ImGui::CloseCurrentPopup();
	}

	const bool bHasCurrentRegion = SelectedSprite && IndexSelectedRegion >= 0 && IndexSelectedRegion < static_cast<int32_t>(SelectedSprite->Regions.size());
	if (ImGui::MenuItem("Copy current region", nullptr, false, bHasCurrentRegion))
	{
		CopiedProperties = SelectedSprite->Regions[IndexSelectedRegion].Properties;
	}
	if (ImGui::MenuItem("Paste metadata", nullptr, false, !CopiedProperties.empty()))
	{
		AddAndClose(CopiedProperties);
	}

	ImGui::EndPopup();
}

void SSpriteMetadata::Draw_CreatePresetPopup()
{
	static constexpr const char* PopupName = "Create metadata preset##CreateMetadataPreset";
	if (bOpenCreatePresetPopup)
	{
		bOpenCreatePresetPopup = false;
		ImGui::OpenPopup(PopupName);
	}
	if (!ImGui::BeginPopupModal(PopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::SetNextItemWidth(320.0f);
	ImGui::InputText("Preset name", NewPresetName, IM_ARRAYSIZE(NewPresetName));
	ImGui::TextDisabled("Directory: %s", MetadataPresetsPath.string().c_str());
	ImGui::SeparatorText("Properties");

	for (int32_t Index = 0; Index < static_cast<int32_t>(NewPresetProperties.size()); ++Index)
	{
		FSpriteProperty& Property = NewPresetProperties[Index];
		ImGui::PushID(Index);

		char PropertyName[128];
		std::strncpy(PropertyName, Property.Name.c_str(), sizeof(PropertyName));
		PropertyName[sizeof(PropertyName) - 1] = '\0';
		ImGui::SetNextItemWidth(180.0f);
		if (ImGui::InputText("Name", PropertyName, IM_ARRAYSIZE(PropertyName)))
		{
			Property.Name = PropertyName;
		}

		int32_t TypeIndex = Property.IndexByType();
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::Combo("Type", &TypeIndex, FSpriteProperty::TypeNames, IM_ARRAYSIZE(FSpriteProperty::TypeNames)))
		{
			Property.TypeByIndex(TypeIndex);
		}

		std::visit([](auto& Value)
			{
				using TValue = std::decay_t<decltype(Value)>;
				if constexpr (std::is_same_v<TValue, bool>)
				{
					ImGui::Checkbox("Default", &Value);
				}
				else if constexpr (std::is_same_v<TValue, int>)
				{
					ImGui::InputInt("Default", &Value);
				}
				else if constexpr (std::is_same_v<TValue, float>)
				{
					ImGui::InputFloat("Default", &Value);
				}
				else if constexpr (std::is_same_v<TValue, std::string>)
				{
					char Buffer[256];
					std::strncpy(Buffer, Value.c_str(), sizeof(Buffer));
					Buffer[sizeof(Buffer) - 1] = '\0';
					if (ImGui::InputText("Default", Buffer, IM_ARRAYSIZE(Buffer)))
					{
						Value = Buffer;
					}
				}
			}, Property.Variant);

		if (ImGui::Button("Remove"))
		{
			NewPresetProperties.erase(NewPresetProperties.begin() + Index);
			ImGui::PopID();
			--Index;
			continue;
		}
		ImGui::Separator();
		ImGui::PopID();
	}

	if (ImGui::Button("Add property"))
	{
		NewPresetProperties.emplace_back("Property", false);
	}
	if (!PresetError.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", PresetError.c_str());
	}
	ImGui::Separator();
	if (ImGui::Button("Save and use", ImVec2(120.0f, 0.0f)))
	{
		const std::vector<FSpriteProperty> Properties = NewPresetProperties;
		if (SaveMetadataPreset())
		{
			ApplyProperties(Properties);
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}
