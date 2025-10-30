#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include <Utils/UI/Draw_ZXColorVideo.h>

struct FSprite
{
	uint32_t Width;
	uint32_t Height;
	std::string Name;

	uint32_t OptionsFlags;
	std::shared_ptr<UI::FZXColorView> ZXColorView;
};

class SSpriteList : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SSpriteList;

public:
	SSpriteList(EFont::Type _FontName);
	virtual ~SSpriteList() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize() override;
	virtual void Render() override;

private:
	void Input_Mouse();

	void Draw_SpriteList();
	bool Draw_ButtonSprite(const char* StringID,
		FSprite& Sprite,
		const ImVec2& VisibleSize,
		const ImVec4& BackgroundColor,
		const ImVec4& TintColor,
		const ImVec4& SelectedColor);

	void AddSprite(
		const ImRect& SpriteRect,
		const std::string& Name,
		int32_t Width, int32_t Height,
		const std::vector<uint8_t>& IndexedData,
		const std::vector<uint8_t>& InkData,
		const std::vector<uint8_t>& AttributeData,
		const std::vector<uint8_t>& MaskData);

	uint32_t ScaleVisible;
	std::vector<FSprite> Sprites;
};
