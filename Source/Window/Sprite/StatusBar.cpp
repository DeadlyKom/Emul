#include "StatusBar.h"
#include <Window/Sprite/Events.h>
#include <Utils/UI/Draw.h>
#include <format>

namespace
{
	static const wchar_t* ThisWindowName = L"Status Bar";
}

SStatusBar::SStatusBar(EFont::Type _FontName, std::string _DockSlot /*= ""*/)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetDockSlot(_DockSlot)
		.SetIncludeInWindows(true))
	, CanvasSize(0.0f, 0.0f)
	, MousePosition(0.0f, 0.0f)
	, CanvasScale(1.0f)
{}

void SStatusBar::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_StatusBar>(
		[this](const FEvent_StatusBar& Event)
		{
			if (Event.Tag == FEventTag::CanvasSizeTag)
			{
				CanvasSize = Event.CanvasSize;
			}
			else if (Event.Tag == FEventTag::MousePositionTag)
			{
				MousePosition = Event.MousePosition;
			}
			// Check whether the active canvas has reported its current zoom.
			else if (Event.Tag == FEventTag::CanvasViewScaleTag)
			{
				CanvasScale = Event.CanvasScale;
			}
		});
}

void SStatusBar::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen);
	{
		Draw_MousePosition();
		// Format the current zoom without trailing zeroes and align it to the right edge.
		const std::string ScaleText = std::format("x{:.5g}", CanvasScale);
		const ImVec2 Padding(0.0f, 0.0f);
		ImGui::SameLine();
		UI::TextAligned(ScaleText.c_str(), { 1.0f, 0.5f }, &Padding);
		ImGui::End();
	}
}

void SStatusBar::Destroy()
{
	UnsubscribeAll();
}

void SStatusBar::Draw_MousePosition()
{
	const int32_t X = FMath::Clamp(FMath::FloorToInt32(MousePosition.x), 0, (int32_t)CanvasSize.x - 1);
	const int32_t Y = FMath::Clamp(FMath::FloorToInt32(MousePosition.y), 0, (int32_t)CanvasSize.y - 1);
	ImGui::Text("Canvas: (%i, %i)", (int32_t)CanvasSize.x, (int32_t)CanvasSize.y);
	ImGui::SameLine();
	ImGui::SetCursorPosX(150.0f);
	ImGui::Text("Pixel: (%i, %i)", X, Y);
	ImGui::SameLine();
	ImGui::SetCursorPosX(250.0f);
	ImGui::Text("Boundary: (%i, %i)", X / 8, Y / 8);
}
