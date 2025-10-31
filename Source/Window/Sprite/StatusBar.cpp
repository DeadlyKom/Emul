#include "StatusBar.h"
#include "Events.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Status Bar";
}

SStatusBar::SStatusBar(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
	, CanvasSize(0.0f, 0.0f)
	, MousePosition(0.0f, 0.0f)
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
		ImGui::End();
	}
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
