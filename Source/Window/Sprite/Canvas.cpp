#include "Canvas.h"
#include "Utils/UI/Draw.h"

namespace
{
	static const char* ThisWindowName = TEXT("Canvas");
}

SCanvas::SCanvas(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
	, bDragging(false)
	, bRefreshCanvas(false)
	, bPressedSource(true)
	, bPressedInk(false)
	, bPressedPaper(false)
	, bPressedMask(false)
	, Width(0)
	, Height(0)
{}

void SCanvas::Initialize()
{
	ZXColorView = std::make_shared<UI::FZXColorView>();

	ZXColorView->Scale = ImVec2(2.5f, 2.5f);
	ZXColorView->ImagePosition = ImVec2(0.0f, 0.0f);

	//uint8_t* ImageData = FImageBase::LoadToMemory("D:\\Work\\[Sprite]\\Геройчики\\Fake\\111.png", Width, Height);
	uint8_t* ImageData = FImageBase::LoadToMemory("D:\\Work\\[Sprite]\\Геройчики\\Tile\\Hex\\Hex1.png", Width, Height);
	UI::QuantizeToZX(ImageData, Width, Height, 4, ZXColorView->IndexedData);
	UI::ZXIndexColorToRGBA(ZXColorView->Image, ZXColorView->IndexedData, Width, Height, true);

	{
		const int32_t TransparentIndex = UI::ZXSpectrumColor::Transparent;
		const int32_t ReplaceTransparent = UI::ZXSpectrumColor::White;
		UI::ZXIndexColorToAttributeRGBA(ZXColorView->IndexedData, Width, Height, ZXColorView->InkData, ZXColorView->AttributeData, TransparentIndex, ReplaceTransparent);
	}

	FImageBase::ReleaseLoadedIntoMemory(ImageData);

	ZXColorView->Device = Data.Device;
	ZXColorView->DeviceContext = Data.DeviceContext;
	Draw_ZXColorView_Initialize(ZXColorView, UI::ERenderType::Screen);
}

void SCanvas::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	if (bRefreshCanvas)
	{
		if (bPressedSource)
		{
			UI::ZXIndexColorToRGBA(ZXColorView->Image, ZXColorView->IndexedData, Width, Height);
		}
		else
		{
			UI::ZXDataToToRGBA(ZXColorView->Image, bPressedInk ? ZXColorView->InkData.data() : nullptr, bPressedPaper ? ZXColorView->AttributeData.data() : nullptr, Width, Height);
		}
		bRefreshCanvas = false;
	}

	ImGui::Begin(ThisWindowName, &bOpen);
	{
		Input_HotKeys();
		Input_Mouse();

		ImGui::Spacing();
		if (ImGui::Button("Options", { 0.0f, 25.0f }))
		{
			ImGui::OpenPopup("Context");
		}
		if (ImGui::BeginPopup("Context"))
		{
			ImGui::Checkbox("Grid", &ZXColorView->Options.bGrid);
			ImGui::Checkbox("Attribute Grid", &ZXColorView->Options.bAttributeGrid);
			ImGui::Checkbox("Alpha Checkerboard", &ZXColorView->Options.bAlphaCheckerboardGrid);
			
			ImGui::SameLine();

			ImGui::ColorEdit4("MyColor##3", (float*)&ZXColorView->TransparentColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha);

			ImGui::EndPopup();
		}

		const float WindowWidth = ImGui::GetWindowContentRegionMax().x;
		const float WidthSource = 25.0f;
		const float WidthInk = 25.0f;
		const float WidthPaper = 25.0f;
		const float WidthMask = 25.0f;
		const float Spacing = ImGui::GetStyle().ItemSpacing.x;
		const float TotalWidth = WidthSource + WidthInk + WidthPaper + WidthMask + Spacing * 2.0f;
		const float StartButtons = WindowWidth - TotalWidth;

		ImGui::SameLine(StartButtons);
		if (UI::Button("S", bPressedSource, { WidthSource, WidthSource }, ZXColorView->IndexedData.size() > 0))
		{
			bPressedSource = !bPressedSource;
			if (bPressedSource)
			{
				bPressedInk = bPressedPaper = bPressedMask = false;
			}
			else
			{
				bPressedInk = bPressedPaper = true;
			}
			bRefreshCanvas = true;
		}
		ImGui::SameLine();
		if (UI::Button("I", bPressedInk, { WidthInk, WidthInk }, ZXColorView->InkData.size() > 0))
		{
			bPressedInk = !bPressedInk;
			if (bPressedInk)
			{
				bPressedSource = bPressedMask = false;
			}
			bRefreshCanvas = true;
		}
		ImGui::SameLine(); 
		if (UI::Button("P", bPressedPaper, { WidthPaper, WidthPaper }, ZXColorView->AttributeData.size() > 0))
		{
			bPressedPaper = !bPressedPaper;
			if (bPressedPaper)
			{
				bPressedSource = bPressedMask = false;
			}
			bRefreshCanvas = true;
		}
		ImGui::SameLine(); 
		if (UI::Button("M", bPressedMask, { WidthMask, WidthMask }, ZXColorView->MaskData.size() > 0))
		{
			bPressedMask = !bPressedMask;
			if (bPressedMask)
			{
				bPressedSource = bPressedInk = bPressedPaper = false;
			}
			bRefreshCanvas = true;
		}

		ImGui::BeginChild("Child", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBringToFrontOnFocus);
		CanvasID = ImGui::GetCurrentWindow()->ID;
		UI::Draw_ZXColorView(ZXColorView);
		ImGui::EndChild();

		ImGui::End();
	}
}

void SCanvas::Destroy()
{
	UI::Draw_ZXColorView_Shutdown(ZXColorView);
}

void SCanvas::Input_HotKeys()
{
}

void SCanvas::Input_Mouse()
{
	bDragging = false;

	ImGuiContext& Context = *ImGui::GetCurrentContext();
	const float MouseWheel = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner) ? Context.IO.MouseWheel : 0.0f;

	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != CanvasID)
	{
		return;
	}

	if (MouseWheel != 0.0f && !Context.IO.FontAllowUserScaling)
	{
		UI::Set_ZXViewScale(ZXColorView, MouseWheel);
	}
	else if (!bDragging && Context.IO.MouseDown[ImGuiMouseButton_Middle])
	{
		bDragging = true;
	}
	else if (Context.IO.MouseReleased[ImGuiMouseButton_Middle])
	{
		bDragging = false;
	}
	// dragging
	if (bDragging)
	{
		UI::Add_ZXViewDeltaPosition(ZXColorView, ImGui::GetIO().MouseDelta);
	}
}
