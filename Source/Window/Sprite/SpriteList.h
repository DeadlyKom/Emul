#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include <Utils/UI/Draw_ZXColorVideo.h>

struct FSprite
{
	uint32_t Width;
	uint32_t Height;
	std::string Name;

	// internal variable
	bool bSelected;
	double HoverStartTime;

	uint32_t OptionsFlags;
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
	bool Draw_ButtonSprite(
		const char* StringID,
		FSprite& Sprite,
		const ImVec2& VisibleSize,
		const ImVec4& BackgroundColor,
		const ImVec4& TintColor,
		const ImVec4& SelectedColor);
	void Draw_ExportSprites();

	void AddSprite(
		const ImRect& SpriteRect,
		const std::string& Name,
		int32_t Width, int32_t Height,
		const std::vector<uint8_t>& IndexedData,
		const std::vector<uint8_t>& InkData,
		const std::vector<uint8_t>& AttributeData,
		const std::vector<uint8_t>& MaskData);

	void ExportSprites(const std::filesystem::path& ScriptFilePath, const std::filesystem::path& ExportPath, std::vector<FSprite> SelectedSprites);

	bool bNeedKeptOpened_ExportPopup;
	uint32_t ScaleVisible;
	ImGuiID SpriteListID;
	std::filesystem::path CurrentPath;
	std::vector<FSprite> Sprites;

	// popup menu 'Export'
	int32_t IndexSelectedScript;
	std::vector<std::string> ScriptFileNames;
	std::map<std::string, std::string> ScriptFiles;
};
