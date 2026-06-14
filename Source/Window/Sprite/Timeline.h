#pragma once

#include <CoreMinimal.h>
#include <Core/ViewerBase.h>
#include <Core/Image.h>
#include <Utils/PropertyBag.h>
#include <Utils/Aseprite/Format.h>

enum class EImageFormat;
struct FKeyframes;

namespace ETimelineAction
{
	enum Type
	{
		FirstFrame,
		PreviousFrame,
		Play,
		NextFrame,
		LastName,
	};
}

enum ETimelineDragMode
{
	TimelineDrag_None,
	TimelineDrag_Cells,
	TimelineDrag_FramesHeader,
	TimelineDrag_LayersHeader
};

struct FTimelineState
{
	int32_t CurrentFrame;
	int32_t CurrentLayer;

	bool bDragging;
	ETimelineDragMode DragMode;

	int32_t DragStartFrame;
	int32_t DragStartLayer;

	int32_t SelMinFrame;
	int32_t SelMaxFrame;
	int32_t SelMinLayer;
	int32_t SelMaxLayer;

	float ScrollX;
	float ScrollY;

	FTimelineState()
		: CurrentFrame(0)
		, CurrentLayer(0)
		, bDragging(false)
		, DragMode(TimelineDrag_None)
		, DragStartFrame(0)
		, DragStartLayer(0)
		, SelMinFrame(INDEX_NONE)
		, SelMaxFrame(INDEX_NONE)
		, SelMinLayer(INDEX_NONE)
		, SelMaxLayer(INDEX_NONE)
		, ScrollX(0.0f)
		, ScrollY(0.0f)
	{}
};

class STimeline : public SViewerChildBase
{
	using Super = SViewerChildBase;
	using ThisClass = STimeline;
	using FTimelineDrawCellContentFn = void (*)(ImDrawList* DrawList, const ImVec2& CellMin, const ImVec2& CellMax, int Frame, int Layer, bool bSelected, bool bCurrentFrame, bool bCurrentLayer, void* UserData);

public:
	STimeline(EFont::Type _FontName, std::string _DockSlot = "", bool bVisible = false);
	virtual ~STimeline() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& Data) override;
	virtual void Initialize(const std::vector<std::any>& Args) override;
	virtual void Render() override;
	virtual void Destroy() override;

private:
	void DrawTimeline(const char* Id, FTimelineState& State, float TimelineHeight = 180.0f, FTimelineDrawCellContentFn DrawCellContent = nullptr, void* DrawCellUserData = nullptr);
	bool ShowModal_ActiveAreaIgnoredPixel();
	void InitializeFromAseprite(std::weak_ptr<AsepriteFormat::FSprite> NewSprite, std::weak_ptr<FKeyframes> SpriteKeyframes);

	// image 
	FImageHandle ImageFirstFrame;
	FImageHandle ImagePreviousFrame;
	FImageHandle ImagePlay;
	FImageHandle ImageNextFrame;
	FImageHandle ImageLastName;

	int32_t FrameCount;
	int32_t LayerCount;
	int32_t PopupFrame;
	int32_t PopupLayer;
	bool bPendingIgnoredPixel00;
	bool bPendingIgnoredPixelFF;
	bool bPendingIgnoredPixelCustom;
	int32_t PendingIgnoredPixelCustomValue;
	EImageFormat Format;

	FTimelineState TimelineState;
	std::weak_ptr<FKeyframes> Keyframes;
	std::weak_ptr<AsepriteFormat::FSprite> AsepriteSprite;
};
