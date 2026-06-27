#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include <set>
#include "SpriteList.h"

class SSpriteMetadata : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SSpriteMetadata;
public:
	SSpriteMetadata(EFont::Type _FontName, std::string _DockSlot = "");
	virtual ~SSpriteMetadata() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize(const std::vector<std::any>& Args) override;
	virtual void Render() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Destroy() override;

private:
	void Draw_Metadata();
	void Draw_Regions(const ImVec2& Size);
	void Draw_Sprite(const ImVec2& Size);
	void Draw_Property(const ImVec2& Size);
	void Draw_Buttons(const ImVec2& Size, float HeightButton);
	void Draw_AddPropertyPopup();
	void Draw_CreatePresetPopup();
	void ApplyProperties(const std::vector<FSpriteProperty>& Properties);
	void AddCustomProperty();
	void CopySelectedProperties(bool bCut);
	void PasteProperties();
	void DeleteSelectedProperties();
	void LoadMetadataPresets();
	bool SaveMetadataPreset();

	struct FMetadataPreset
	{
		std::string Name;
		std::vector<FSpriteProperty> Properties;
		std::filesystem::path FilePath;
	};

	float HeverTooltip;
	int32_t IndexSelectedRegion;
	int32_t IndexSelectedProperty;
	int32_t PropertySelectionAnchor = INDEX_NONE;
	std::set<int32_t> SelectedPropertyIndices;
	int32_t MetadataApplyTarget = 0;
	std::shared_ptr<FSprite> SelectedSprite;
	std::unordered_map<std::string, bool> EditingProperty;
	std::vector<FSpriteProperty> CopiedProperties;
	std::filesystem::path MetadataPresetsPath;
	std::vector<FMetadataPreset> MetadataPresets;
	std::vector<FSpriteProperty> NewPresetProperties;
	char NewPresetName[128]{};
	bool bOpenCreatePresetPopup = false;
	std::string PresetError;
};
