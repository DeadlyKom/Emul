#include "ToolBar.h"
#include <Window/Sprite/Events.h>
#include "resource.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Tool Bar";
}

SToolBar::SToolBar(EFont::Type _FontName, std::string _DockSlot /*= ""*/)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetDockSlot(_DockSlot)
		.SetIncludeInWindows(true))
{
	ToolMode[0] = EToolMode::None;
	ToolMode[1] = EToolMode::None;
}

void SToolBar::NativeInitialize(const FNativeDataInitialize& Data)
{
	Super::NativeInitialize(Data);

	SubscribeEvent<FEvent_Canvas>(
		[this](const FEvent_Canvas& Event)
		{
			if (Event.Tag == FEventTag::ChangeToolModeTag)
			{
				SetToolMode(Event.ChangeToolMode.ToolMode, true, true);
			}
		});
}

void SToolBar::Initialize(const std::any& Arg)
{
	ImageRectangleMarquee = FImageBase::LoadImageFromResource(IDB_RECTANGLE_MARQUEE, TEXT("PNG")).Handle;
	ImagePencil = FImageBase::LoadImageFromResource(IDB_PENCIL, TEXT("PNG")).Handle;
	ImageEraser = FImageBase::LoadImageFromResource(IDB_ERASER, TEXT("PNG")).Handle;
	ImageEyedropper = FImageBase::LoadImageFromResource(IDB_EYEDROPPER, TEXT("PNG")).Handle;
	ImagePaintBucket = FImageBase::LoadImageFromResource(IDB_PAINTBUCKET, TEXT("PNG")).Handle;
}

void SToolBar::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(50.0f, 65.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.0f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1.0f, 1.0f));

	ImGui::Begin(GetWindowName().c_str(), &bOpen);
	{
		const float DefaultWidth = ImGui::GetContentRegionAvail().x;
		auto ButtonLambda = [DefaultWidth](const char* ID, FImageHandle ImageHandle, bool bSelectedCondition, float& AvailWidth) -> bool
		{
			FImageBase& Images = FImageBase::Get();
			FImage& Image = Images.GetImage(ImageHandle);
			if (!Image.IsValid())
			{
				return false;
			}

			const ImVec2 Padding = ImGui::GetStyle().FramePadding;
			const ImVec4 BackgroundColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
			const ImVec4 TintColor = ImVec4(1.0f, 1.0f, 1.0f, 0.5f);
			const ImVec4 SelectedColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
			const float Width = Image.Size.x + Padding.x * 2.0f;

			const bool bResult = ImGui::ImageButton(ID, Image.ShaderResourceView, Image.Size, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), BackgroundColor, bSelectedCondition ? SelectedColor : TintColor);

			AvailWidth -= Width;
			if (AvailWidth > Width)
			{
				ImGui::SameLine();
			}
			else if (AvailWidth < Width)
			{
				AvailWidth = DefaultWidth;
			}

			return bResult;
		};

		float AvailWidth = DefaultWidth;
		if (ButtonLambda("Tools##RectangleMarquee", ImageRectangleMarquee, IsEqualToolMode(EToolMode::RectangleMarquee), AvailWidth))
		{
			SetToolMode(EToolMode::RectangleMarquee, true);
		}
		if (ButtonLambda("Tools##Pencil", ImagePencil, IsEqualToolMode(EToolMode::Pencil), AvailWidth))
		{
			SetToolMode(EToolMode::Pencil, true);
		}
		if (ButtonLambda("Tools##Eraser", ImageEraser, IsEqualToolMode(EToolMode::Eraser), AvailWidth))
		{
			SetToolMode(EToolMode::Eraser, true);
		}
		if (ButtonLambda("Tools##Eyedropper", ImageEyedropper, IsEqualToolMode(EToolMode::Eyedropper), AvailWidth))
		{
			SetToolMode(EToolMode::Eyedropper, true);
		}
		if (ButtonLambda("Tools##PaintBucket", ImagePaintBucket, IsEqualToolMode(EToolMode::PaintBucket), AvailWidth))
		{
			SetToolMode(EToolMode::PaintBucket, true);
		}

		//ImGui::Text("%i, %i", ToolMode[0], ToolMode[1]);

		ImGui::End();
		ImGui::PopStyleVar(3);
	}
}

void SToolBar::Destroy()
{
	UnsubscribeAll();
}

void SToolBar::SetToolMode(EToolMode::Type NewToolMode, bool bForce /*= true*/, bool bEvent /*= false*/)
{
	if (ToolMode[0] != NewToolMode)
	{
		ToolMode[1] = bForce ? EToolMode::None : ToolMode[0];
		ToolMode[0] = NewToolMode;
	}
	else if (ToolMode[1] != EToolMode::None)
	{
		if (ToolMode[1] != NewToolMode)
		{
			ToolMode[0] = ToolMode[1];
		}
		else
		{
			ToolMode[0] = EToolMode::None;
		}
		ToolMode[1] = EToolMode::None;
	}

	if (!bEvent)
	{
		FEvent_ToolBar Event;
		Event.Tag = FEventTag::ChangeToolModeTag;
		Event.ChangeToolMode.ToolMode = NewToolMode;
		SendEvent(Event);
	}
}
