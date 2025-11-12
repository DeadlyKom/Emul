#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>

struct FSprite;

class SSpriteMetadata : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = SSpriteMetadata;
public:
	SSpriteMetadata(EFont::Type _FontName, std::string _DockSlot = "");
	virtual ~SSpriteMetadata() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Render() override;
	virtual void Tick(float DeltaTime) override;
	virtual void Destroy() override;

private:
	void Draw_Metadata();
	void Draw_Regions(const ImVec2& Size);
	void Draw_Sprite(const ImVec2& Size);
	void Draw_Property(const ImVec2& Size);
	void Draw_Buttons(const ImVec2& Size, float HeightButton);

	float HeverTooltip;
	int32_t IndexSelectedRegion;
	int32_t IndexSelectedProperty;
	std::shared_ptr<FSprite> SelectedSprite;
	std::unordered_map<std::string, bool> EditingProperty;
};
