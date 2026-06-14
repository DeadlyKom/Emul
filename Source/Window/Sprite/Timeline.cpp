#include "Timeline.h"
#include <Window/Sprite/Events.h>
#include <Utils/UI/Draw.h>
#include <Utils/UI/Draw_ZXColorVideo.h>
#include "resource.h"
#include "Keyframes.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Timeline";

	static int32_t ClampInt(int32_t v, int32_t min, int32_t max)
	{
		return max(min, min(v, max));
	}

    static float ClampFloat(float Value, float Min, float Max)
    {
        return max(Min, min(Value, Max));
    }

    static int32_t CeilToInt(float Value)
    {
        return (int32_t)ceilf(Value);
    }

    static int32_t FloorToInt(float Value)
    {
        return (int32_t)floorf(Value);
    }

	static bool IsSelected(const FTimelineState& State, int32_t Frame, int32_t Layer)
	{
		return Frame >= State.SelMinFrame &&
			Frame <= State.SelMaxFrame &&
			Layer >= State.SelMinLayer &&
			Layer <= State.SelMaxLayer;
	}

    static bool IsPointInsideRect(const ImVec2& Point, const ImVec2& Min, const ImVec2& Max)
    {
        return Point.x >= Min.x &&
            Point.y >= Min.y &&
            Point.x < Max.x &&
            Point.y < Max.y;
    }

    static void DrawExampleCellContent(ImDrawList* DrawList, const ImVec2& CellMin, const ImVec2& CellMax, int32_t Frame, int32_t Layer, bool bSelected, bool bCurrentFrame, bool bCurrentLayer, void* UserData)
    {
        FKeyframes* Data = (FKeyframes*)UserData;
        if (Data == nullptr)
        {
            return;
        }

        const FPropertyBag& Property = Data->GetProperty(Frame, Layer);
        if (!Property.IsValid())
        {
            return;
        }

        const bool bHasLimitArea = Property.HasProperty(FPropertyTag::LimitArea);
        if (!bHasLimitArea)
        {
            return;
        }

        FTilemapCellData_Rect LimitArea;
        Property.GetStruct(FPropertyTag::LimitArea, LimitArea);

        const ImU32 ColKey = IM_COL32(255, 220, 80, 255);
        const ImVec2 Center((CellMin.x + CellMax.x) * 0.5f, (CellMin.y + CellMax.y) * 0.5f);
        if (LimitArea.bActiveArea)
        {
            DrawList->AddCircle(Center, 5.0f, ColKey, 12, 2.0f);
        }
        else
        {
            DrawList->AddCircleFilled(Center, 4.0f, ColKey, 12);
        }
    }
}

STimeline::STimeline(EFont::Type _FontName, std::string _DockSlot /*= ""*/, bool bVisible /*= false*/)
	: Super(FWindowInitializer()
        .SetOpen(bVisible)
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetDockSlot(_DockSlot)
		.SetIncludeInWindows(true))
    , FrameCount(0)
    , LayerCount(0)
    , PopupFrame(INDEX_NONE)
    , PopupLayer(INDEX_NONE)
{}

void STimeline::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

    SubscribeEvent<FEvent_Timeline>(
        [this](const FEvent_Timeline& Event)
        {
            if (Event.Tag == FEventTag::TimelineInitializeTag)
            {
                Format = Event.Format;
                if (Event.Format == EImageFormat::Aseprite)
                {
                    InitializeFromAseprite(Event.Sprite, Event.Keyframes);
                }
            }
            else if (Event.Tag == FEventTag::TimelineChangedFrameTag)
            {
                TimelineState.SelMinFrame = Event.Frame;
                TimelineState.SelMaxFrame = Event.Frame;
            }
        });

    SubscribeEvent<FEvent_RequestTimelineState>(
        [this](const FEvent_RequestTimelineState& Event)
        {
            if (Event.Tag == FEventTag::RequestTimelineStateTag)
            {
                Event.ExecuteCallback(TimelineState);
            }
        });
}

void STimeline::Initialize(const std::vector<std::any>& Args)
{
	// ToDo
	//ImageFirstFrame = FImageBase::LoadImageFromResource(IDB_RECTANGLE_MARQUEE, TEXT("PNG")).Handle;
	//ImagePreviousFrame = FImageBase::LoadImageFromResource(IDB_PENCIL, TEXT("PNG")).Handle;
	//ImagePlay = FImageBase::LoadImageFromResource(IDB_ERASER, TEXT("PNG")).Handle;
	//ImageNextFrame = FImageBase::LoadImageFromResource(IDB_EYEDROPPER, TEXT("PNG")).Handle;
	//ImageLastName = FImageBase::LoadImageFromResource(IDB_PAINTBUCKET, TEXT("PNG")).Handle;
}

void STimeline::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen);
	{
        // timeline panel
        {
            ImVec2 Avail = ImGui::GetContentRegionAvail();
            ImGui::BeginChild(
                "TimelineFrame",
                ImVec2(0.0f, Avail.y),
                true,
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse
            );

            ImVec2 ChildAvail = ImGui::GetContentRegionAvail();
            float TimelineHeight = ChildAvail.y;
            if (TimelineHeight < 80.0f)
            {
                TimelineHeight = 80.0f;
            }

            std::shared_ptr<FKeyframes> _Keyframes = Keyframes.lock();
            if (_Keyframes)
            {
                DrawTimeline(
                    "SpriteTimeline",
                    TimelineState,
                    TimelineHeight,
                    DrawExampleCellContent,
                    _Keyframes.get()
                );
            }

            ImGui::EndChild();
        }
		ImGui::End();
	}
}

void STimeline::Destroy()
{
	UnsubscribeAll();
}

void STimeline::DrawTimeline(const char* Id, FTimelineState& State, float TimelineHeight, FTimelineDrawCellContentFn DrawCellContent, void* DrawCellUserData)
{
    if (FrameCount <= 0 || LayerCount <= 0)
    {
        return;
    }

    const float CellW = 28.0f;
    const float CellH = 22.0f;

    const float HeaderH = 22.0f;
    const float LayerNameW = 100.0f;

    const float TableW = FrameCount * CellW;
    const float TableH = LayerCount * CellH;

    const float ScrollbarSize = ImGui::GetStyle().ScrollbarSize;

    const ImU32 ColBg = IM_COL32(35, 35, 35, 255);
    const ImU32 ColHeader = IM_COL32(50, 50, 50, 255);
    const ImU32 ColHeaderSelected = IM_COL32(75, 85, 105, 255);
    const ImU32 ColCell = IM_COL32(45, 45, 45, 255);
    const ImU32 ColCellAlt = IM_COL32(40, 40, 40, 255);
    const ImU32 ColGrid = IM_COL32(75, 75, 75, 255);
    const ImU32 ColSelected = IM_COL32(90, 120, 180, 180);
    const ImU32 ColCurrent = IM_COL32(255, 190, 40, 255);
    const ImU32 ColText = IM_COL32(220, 220, 220, 255);

    ImGui::PushID(Id);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    ImGui::BeginChild(
        "TimelineRoot",
        ImVec2(0.0f, TimelineHeight),
        true,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse
    );

    ImVec2 RootCursor = ImGui::GetCursorPos();
    ImVec2 RootScreen = ImGui::GetCursorScreenPos();
    ImVec2 RootAvail = ImGui::GetContentRegionAvail();

    ImGuiIO& IO = ImGui::GetIO();
    ImVec2 Mouse = IO.MousePos;

    ImVec2 RootWindowMin = ImGui::GetWindowPos();
    ImVec2 RootWindowMax(
        RootWindowMin.x + ImGui::GetWindowSize().x,
        RootWindowMin.y + ImGui::GetWindowSize().y
    );

    bool bMouseInsideTimeline = IsPointInsideRect(
        Mouse,
        RootWindowMin,
        RootWindowMax
    );

    bool bTimelineHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    bool bCanAcceptMouse = bMouseInsideTimeline && bTimelineHovered;

    float RootW = max(1.0f, RootAvail.x);
    float RootH = max(HeaderH + CellH, RootAvail.y);

    float HeaderW = max(1.0f, RootW - LayerNameW);
    float BodyH = max(1.0f, RootH - HeaderH);

    bool bNeedVerticalScrollbar = TableH > BodyH;
    bool bNeedHorizontalScrollbar = TableW > HeaderW;

    if (bNeedVerticalScrollbar && TableW > HeaderW - ScrollbarSize)
    {
        bNeedHorizontalScrollbar = true;
    }

    if (bNeedHorizontalScrollbar && TableH > BodyH - ScrollbarSize)
    {
        bNeedVerticalScrollbar = true;
    }

    float BodyViewW = HeaderW;
    float BodyViewH = BodyH;

    if (bNeedVerticalScrollbar)
    {
        BodyViewW -= ScrollbarSize;
    }

    if (bNeedHorizontalScrollbar)
    {
        BodyViewH -= ScrollbarSize;
    }

    BodyViewW = max(1.0f, BodyViewW);
    BodyViewH = max(1.0f, BodyViewH);

    ImVec2 CornerMin(
        RootScreen.x,
        RootScreen.y
    );

    ImVec2 CornerMax(
        RootScreen.x + LayerNameW,
        RootScreen.y + HeaderH
    );

    ImVec2 FrameHeaderMin(
        RootScreen.x + LayerNameW,
        RootScreen.y
    );

    ImVec2 FrameHeaderMax(
        RootScreen.x + LayerNameW + BodyViewW,
        RootScreen.y + HeaderH
    );

    ImVec2 LayerNamesMin(
        RootScreen.x,
        RootScreen.y + HeaderH
    );

    ImVec2 LayerNamesMax(
        RootScreen.x + LayerNameW,
        RootScreen.y + HeaderH + BodyViewH
    );

    auto SendSelectedFrameEvent = [&](int32_t Frame)
        {
            FEvent_SelectedSprite Event(FEventTag::SelectedSpritesChangedFrameTag);
            Event.Format = Format;
            Event.Frame = Frame;
            SendEvent(Event);
        };

    auto SetCurrentFrame = [&](int32_t Frame)
        {
            Frame = ClampInt(Frame, 0, FrameCount - 1);

            if (State.CurrentFrame != Frame)
            {
                State.CurrentFrame = Frame;
                SendSelectedFrameEvent(Frame);
            }
            else
            {
                State.CurrentFrame = Frame;
            }
        };

    auto SetCurrentLayer = [&](int32_t Layer)
        {
            Layer = ClampInt(Layer, 0, LayerCount - 1);
            State.CurrentLayer = Layer;
        };

    auto GetFrameFromHeaderMouse = [&]()
        {
            float LocalX = Mouse.x - FrameHeaderMin.x + State.ScrollX;

            int32_t Frame = (int32_t)(LocalX / CellW);
            Frame = ClampInt(Frame, 0, FrameCount - 1);

            return Frame;
        };

    auto GetLayerFromHeaderMouse = [&]()
        {
            float LocalY = Mouse.y - LayerNamesMin.y + State.ScrollY;

            int32_t Layer = (int32_t)(LocalY / CellH);
            Layer = ClampInt(Layer, 0, LayerCount - 1);

            return Layer;
        };

    // ----------------------------------------------------
    // header input
    // ----------------------------------------------------

    if (bCanAcceptMouse &&
        !State.bDragging &&
        IsPointInsideRect(Mouse, CornerMin, CornerMax) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        State.SelMinFrame = 0;
        State.SelMaxFrame = FrameCount - 1;
        State.SelMinLayer = 0;
        State.SelMaxLayer = LayerCount - 1;
    }

    if (bCanAcceptMouse &&
        !State.bDragging &&
        IsPointInsideRect(Mouse, FrameHeaderMin, FrameHeaderMax) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        int32_t Frame = GetFrameFromHeaderMouse();

        State.bDragging = true;
        State.DragMode = TimelineDrag_FramesHeader;

        State.DragStartFrame = Frame;

        SetCurrentFrame(Frame);

        State.SelMinFrame = Frame;
        State.SelMaxFrame = Frame;
        State.SelMinLayer = 0;
        State.SelMaxLayer = LayerCount - 1;
    }

    if (bCanAcceptMouse &&
        !State.bDragging &&
        IsPointInsideRect(Mouse, LayerNamesMin, LayerNamesMax) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        int32_t Layer = GetLayerFromHeaderMouse();

        State.bDragging = true;
        State.DragMode = TimelineDrag_LayersHeader;

        State.DragStartLayer = Layer;

        SetCurrentLayer(Layer);

        State.SelMinFrame = 0;
        State.SelMaxFrame = FrameCount - 1;
        State.SelMinLayer = Layer;
        State.SelMaxLayer = Layer;
    }

    if (State.bDragging &&
        State.DragMode == TimelineDrag_FramesHeader &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        int32_t Frame = GetFrameFromHeaderMouse();

        SetCurrentFrame(Frame);

        State.SelMinFrame = min(State.DragStartFrame, Frame);
        State.SelMaxFrame = max(State.DragStartFrame, Frame);
        State.SelMinLayer = 0;
        State.SelMaxLayer = LayerCount - 1;
    }

    if (State.bDragging &&
        State.DragMode == TimelineDrag_LayersHeader &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        int32_t Layer = GetLayerFromHeaderMouse();

        SetCurrentLayer(Layer);

        State.SelMinFrame = 0;
        State.SelMaxFrame = FrameCount - 1;
        State.SelMinLayer = min(State.DragStartLayer, Layer);
        State.SelMaxLayer = max(State.DragStartLayer, Layer);
    }

    if (State.bDragging &&
        State.DragMode != TimelineDrag_Cells &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        State.bDragging = false;
        State.DragMode = TimelineDrag_None;
    }

    // ----------------------------------------------------
    // body
    // ----------------------------------------------------

    ImGui::SetCursorPos(
        ImVec2(
            RootCursor.x + LayerNameW,
            RootCursor.y + HeaderH
        )
    );

    ImGui::BeginChild(
        "FrameBody",
        ImVec2(HeaderW, BodyH),
        false,
        ImGuiWindowFlags_HorizontalScrollbar
    );

    {
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        ImVec2 CanvasPos = ImGui::GetCursorScreenPos();
        ImVec2 CanvasSize(TableW, TableH);

        ImGui::InvisibleButton("FrameBodyCanvas", CanvasSize);

        bool bHovered = ImGui::IsItemHovered();
        bool bActive = ImGui::IsItemActive();

        Mouse = ImGui::GetIO().MousePos;

        auto IsMouseInsideTable = [&]()
            {
                float LocalX = Mouse.x - CanvasPos.x;
                float LocalY = Mouse.y - CanvasPos.y;

                return LocalX >= 0.0f &&
                    LocalY >= 0.0f &&
                    LocalX < TableW &&
                    LocalY < TableH;
            };

        auto GetFrameFromBodyMouse = [&]()
            {
                float LocalX = Mouse.x - CanvasPos.x;

                int32_t Frame = (int32_t)(LocalX / CellW);
                Frame = ClampInt(Frame, 0, FrameCount - 1);

                return Frame;
            };

        auto GetLayerFromBodyMouse = [&]()
            {
                float LocalY = Mouse.y - CanvasPos.y;

                int32_t Layer = (int32_t)(LocalY / CellH);
                Layer = ClampInt(Layer, 0, LayerCount - 1);

                return Layer;
            };

        if (!State.bDragging &&
            bHovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            IsMouseInsideTable())
        {
            int32_t Frame = GetFrameFromBodyMouse();
            int32_t Layer = GetLayerFromBodyMouse();

            State.bDragging = true;
            State.DragMode = TimelineDrag_Cells;

            State.DragStartFrame = Frame;
            State.DragStartLayer = Layer;

            State.SelMinFrame = Frame;
            State.SelMaxFrame = Frame;
            State.SelMinLayer = Layer;
            State.SelMaxLayer = Layer;

            SetCurrentFrame(Frame);
            SetCurrentLayer(Layer);
        }

        if (!State.bDragging &&
            bHovered &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
            IsMouseInsideTable())
        {
            const int32_t Frame = GetFrameFromBodyMouse();
            const int32_t Layer = GetLayerFromBodyMouse();
            std::shared_ptr<FKeyframes> KeyframeData = Keyframes.lock();
            if (KeyframeData && KeyframeData->GetProperty(Frame, Layer).IsValid())
            {
                PopupFrame = Frame;
                PopupLayer = Layer;
                ImGui::OpenPopup("TimelineCellPropertyPopup");
            }
        }

        if (ImGui::BeginPopup("TimelineCellPropertyPopup"))
        {
            std::shared_ptr<FKeyframes> KeyframeData = Keyframes.lock();
            FPropertyBag* Property = KeyframeData ? KeyframeData->GetMutableProperty(PopupFrame, PopupLayer) : nullptr;
            if (Property != nullptr)
            {
                const bool bHasLimitArea = Property->HasProperty(FPropertyTag::LimitArea);
                if (bHasLimitArea)
                {
                    if (ImGui::MenuItem("Delete Limit Area"))
                    {
                        Property->RemoveProperty(FPropertyTag::LimitArea);
                    }

                    FTilemapCellData_Rect LimitArea;
                    if (Property->GetStruct(FPropertyTag::LimitArea, LimitArea))
                    {
                        if (LimitArea.bActiveArea && ImGui::MenuItem("Convert to Limit Area"))
                        {
                            LimitArea.bActiveArea = false;
                            Property->SetStruct(FPropertyTag::LimitArea, LimitArea);
                        }
                        else if (!LimitArea.bActiveArea && ImGui::MenuItem("Convert to Active Area"))
                        {
                            LimitArea.bActiveArea = true;
                            Property->SetStruct(FPropertyTag::LimitArea, LimitArea);
                        }
                    }
                }
            }
            ImGui::EndPopup();
        }

        if (State.bDragging &&
            State.DragMode == TimelineDrag_Cells &&
            bActive &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
            IsMouseInsideTable())
        {
            int32_t Frame = GetFrameFromBodyMouse();
            int32_t Layer = GetLayerFromBodyMouse();

            State.SelMinFrame = min(State.DragStartFrame, Frame);
            State.SelMaxFrame = max(State.DragStartFrame, Frame);

            State.SelMinLayer = min(State.DragStartLayer, Layer);
            State.SelMaxLayer = max(State.DragStartLayer, Layer);

            SetCurrentFrame(Frame);
            SetCurrentLayer(Layer);
        }

        if (State.bDragging &&
            State.DragMode == TimelineDrag_Cells &&
            ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            State.bDragging = false;
            State.DragMode = TimelineDrag_None;
        }

        State.ScrollX = ImGui::GetScrollX();
        State.ScrollY = ImGui::GetScrollY();

        ImVec2 WindowPos = ImGui::GetWindowPos();

        ImVec2 ClipMin = WindowPos;
        ImVec2 ClipMax(
            WindowPos.x + BodyViewW,
            WindowPos.y + BodyViewH
        );

        DrawList->PushClipRect(ClipMin, ClipMax, true);

        DrawList->AddRectFilled(
            CanvasPos,
            ImVec2(CanvasPos.x + TableW, CanvasPos.y + TableH),
            ColBg
        );

        int32_t FirstVisibleFrame = ClampInt(
            FloorToInt(State.ScrollX / CellW) - 1,
            0,
            FrameCount - 1
        );

        int32_t LastVisibleFrame = ClampInt(
            CeilToInt((State.ScrollX + BodyViewW) / CellW) + 1,
            0,
            FrameCount - 1
        );

        int32_t FirstVisibleLayer = ClampInt(
            FloorToInt(State.ScrollY / CellH) - 1,
            0,
            LayerCount - 1
        );

        int32_t LastVisibleLayer = ClampInt(
            CeilToInt((State.ScrollY + BodyViewH) / CellH) + 1,
            0,
            LayerCount - 1
        );

        for (int32_t Layer = FirstVisibleLayer; Layer <= LastVisibleLayer; ++Layer)
        {
            float Y = CanvasPos.y + Layer * CellH;

            for (int32_t Frame = FirstVisibleFrame; Frame <= LastVisibleFrame; ++Frame)
            {
                float X = CanvasPos.x + Frame * CellW;

                ImVec2 P0(X, Y);
                ImVec2 P1(X + CellW, Y + CellH);

                ImU32 BaseCol = Layer % 2 == 0 ? ColCell : ColCellAlt;

                DrawList->AddRectFilled(P0, P1, BaseCol);

                bool bSelected = IsSelected(State, Frame, Layer);
                bool bCurrentFrame = Frame == State.CurrentFrame;
                bool bCurrentLayer = Layer == State.CurrentLayer;

                if (bSelected)
                {
                    DrawList->AddRectFilled(P0, P1, ColSelected);
                }

                if (DrawCellContent != nullptr)
                {
                    DrawCellContent(
                        DrawList,
                        P0,
                        P1,
                        Frame,
                        Layer,
                        bSelected,
                        bCurrentFrame,
                        bCurrentLayer,
                        DrawCellUserData
                    );
                }

                DrawList->AddRect(P0, P1, ColGrid);
            }
        }

        {
            int32_t CurrentLayer = ClampInt(State.CurrentLayer, 0, LayerCount - 1);

            float Y = CanvasPos.y + CurrentLayer * CellH;

            DrawList->AddRect(
                ImVec2(CanvasPos.x, Y),
                ImVec2(CanvasPos.x + TableW, Y + CellH),
                ColCurrent,
                0.0f,
                0,
                2.0f
            );
        }

        {
            int32_t CurrentFrame = ClampInt(State.CurrentFrame, 0, FrameCount - 1);

            float X = CanvasPos.x + CurrentFrame * CellW;

            DrawList->AddRect(
                ImVec2(X, CanvasPos.y),
                ImVec2(X + CellW, CanvasPos.y + TableH),
                ColCurrent,
                0.0f,
                0,
                2.0f
            );
        }

        DrawList->PopClipRect();
    }

    ImGui::EndChild();

    // ----------------------------------------------------
    // corner
    // ----------------------------------------------------

    ImGui::SetCursorPos(RootCursor);

    ImGui::BeginChild(
        "Corner",
        ImVec2(LayerNameW, HeaderH),
        false,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse
    );

    {
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        ImVec2 P0 = ImGui::GetCursorScreenPos();
        ImVec2 P1(P0.x + LayerNameW, P0.y + HeaderH);

        ImU32 FillCol = ColHeader;

        if (State.SelMinFrame == 0 &&
            State.SelMaxFrame == FrameCount - 1 &&
            State.SelMinLayer == 0 &&
            State.SelMaxLayer == LayerCount - 1)
        {
            FillCol = ColHeaderSelected;
        }

        DrawList->AddRectFilled(P0, P1, FillCol);
        DrawList->AddRect(P0, P1, ColGrid);

        DrawList->AddText(
            ImVec2(P0.x + 6.0f, P0.y + 3.0f),
            ColText,
            "Layers"
        );

        ImGui::Dummy(ImVec2(LayerNameW, HeaderH));
    }

    ImGui::EndChild();

    // ----------------------------------------------------
    // frame header
    // ----------------------------------------------------

    ImGui::SetCursorPos(
        ImVec2(
            RootCursor.x + LayerNameW,
            RootCursor.y
        )
    );

    ImGui::BeginChild(
        "FrameHeader",
        ImVec2(HeaderW, HeaderH),
        false,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse
    );

    {
        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        ImVec2 CanvasPos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton(
            "FrameHeaderCanvas",
            ImVec2(HeaderW, HeaderH)
        );

        ImVec2 WindowPos = ImGui::GetWindowPos();

        ImVec2 ClipMin = WindowPos;
        ImVec2 ClipMax(
            WindowPos.x + BodyViewW,
            WindowPos.y + HeaderH
        );

        DrawList->PushClipRect(ClipMin, ClipMax, true);

        DrawList->AddRectFilled(
            CanvasPos,
            ImVec2(CanvasPos.x + BodyViewW, CanvasPos.y + HeaderH),
            ColBg
        );

        int32_t FirstVisibleFrame = ClampInt(
            FloorToInt(State.ScrollX / CellW) - 1,
            0,
            FrameCount - 1
        );

        int32_t LastVisibleFrame = ClampInt(
            CeilToInt((State.ScrollX + BodyViewW) / CellW) + 1,
            0,
            FrameCount - 1
        );

        for (int32_t Frame = FirstVisibleFrame; Frame <= LastVisibleFrame; ++Frame)
        {
            float X = CanvasPos.x - State.ScrollX + Frame * CellW;

            ImVec2 P0(X, CanvasPos.y);
            ImVec2 P1(X + CellW, CanvasPos.y + HeaderH);

            ImU32 FillCol = ColHeader;

            if (Frame >= State.SelMinFrame && Frame <= State.SelMaxFrame)
            {
                FillCol = ColHeaderSelected;
            }

            DrawList->AddRectFilled(P0, P1, FillCol);
            DrawList->AddRect(P0, P1, ColGrid);

            char Buffer[16];
            snprintf(Buffer, sizeof(Buffer), "%d", Frame + 1);

            ImVec2 TextSize = ImGui::CalcTextSize(Buffer);

            DrawList->AddText(
                ImVec2(
                    X + (CellW - TextSize.x) * 0.5f,
                    CanvasPos.y + (HeaderH - TextSize.y) * 0.5f
                ),
                ColText,
                Buffer
            );
        }

        {
            int32_t CurrentFrame = ClampInt(State.CurrentFrame, 0, FrameCount - 1);

            float X = CanvasPos.x - State.ScrollX + CurrentFrame * CellW;

            DrawList->AddRect(
                ImVec2(X, CanvasPos.y),
                ImVec2(X + CellW, CanvasPos.y + HeaderH),
                ColCurrent,
                0.0f,
                0,
                2.0f
            );
        }

        DrawList->PopClipRect();
    }

    ImGui::EndChild();

    // ----------------------------------------------------
    // layer names
    // ----------------------------------------------------

    ImGui::SetCursorPos(
        ImVec2(
            RootCursor.x,
            RootCursor.y + HeaderH
        )
    );

    ImGui::BeginChild(
        "LayerNames",
        ImVec2(LayerNameW, BodyH),
        false,
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse
    );

    {
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        ImVec2 CanvasPos = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton(
            "LayerNamesCanvas",
            ImVec2(LayerNameW, BodyH)
        );

        ImVec2 WindowPos = ImGui::GetWindowPos();

        ImVec2 ClipMin = WindowPos;
        ImVec2 ClipMax(
            WindowPos.x + LayerNameW,
            WindowPos.y + BodyViewH
        );

        DrawList->PushClipRect(ClipMin, ClipMax, true);

        DrawList->AddRectFilled(
            CanvasPos,
            ImVec2(CanvasPos.x + LayerNameW, CanvasPos.y + BodyViewH),
            ColBg
        );

        int32_t FirstVisibleLayer = ClampInt(
            FloorToInt(State.ScrollY / CellH) - 1,
            0,
            LayerCount - 1
        );

        int32_t LastVisibleLayer = ClampInt(
            CeilToInt((State.ScrollY + BodyViewH) / CellH) + 1,
            0,
            LayerCount - 1
        );

        std::shared_ptr<AsepriteFormat::FSprite> Sprite = AsepriteSprite.lock();

        for (int32_t Layer = FirstVisibleLayer; Layer <= LastVisibleLayer; ++Layer)
        {
            float Y = CanvasPos.y - State.ScrollY + Layer * CellH;

            ImVec2 P0(CanvasPos.x, Y);
            ImVec2 P1(CanvasPos.x + LayerNameW, Y + CellH);

            ImU32 FillCol = ColHeader;

            if (Layer >= State.SelMinLayer && Layer <= State.SelMaxLayer)
            {
                FillCol = ColHeaderSelected;
            }

            DrawList->AddRectFilled(P0, P1, FillCol);
            DrawList->AddRect(P0, P1, ColGrid);

            if (Sprite)
            {
                int32_t SpriteLayerIndex = (int32_t)Sprite->Layers.size() - Layer - 1;

                if (SpriteLayerIndex >= 0 &&
                    SpriteLayerIndex < (int32_t)Sprite->Layers.size())
                {
                    DrawList->AddText(
                        ImVec2(P0.x + 6.0f, P0.y + 3.0f),
                        ColText,
                        Sprite->Layers[SpriteLayerIndex].Name.c_str()
                    );
                }
            }

            if (Layer == State.CurrentLayer)
            {
                DrawList->AddRect(
                    P0,
                    P1,
                    ColCurrent,
                    0.0f,
                    0,
                    2.0f
                );
            }
        }

        DrawList->PopClipRect();
    }

    ImGui::EndChild();

    ImGui::EndChild();

    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

void STimeline::InitializeFromAseprite(std::weak_ptr<AsepriteFormat::FSprite> NewSprite, std::weak_ptr<FKeyframes> SpriteKeyframes)
{
    AsepriteSprite = NewSprite;
    Keyframes = SpriteKeyframes;
    std::shared_ptr<AsepriteFormat::FSprite> Sprite = NewSprite.lock();
    if (!Sprite)
    {
        return;
    }

    FrameCount = (int32_t)Sprite->Frames.size();
    LayerCount = (int32_t)Sprite->Layers.size();
}
