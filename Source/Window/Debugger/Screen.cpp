#include "Screen.h"

#include "AppDebugger.h"
#include <Core/Image.h>
#include <Utils/UI/Draw.h>
#include "Motherboard/Motherboard.h"
#include "Devices/ControlUnit/Interface_Display.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Screen";
	static const char* ResourcePathName = TEXT("SHADER/ZX");
}

SScreen::SScreen(EFont::Type _FontName)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(_FontName)
		.SetIncludeInWindows(true))
	, bDragging(false)										// is user currently dragging to pan view
{}

void SScreen::Initialize(const std::any& Arg)
{
	ZXColorView = std::make_shared<UI::FZXColorView>();

	const FSpectrumDisplay SD = GetMotherboard().GetState<FSpectrumDisplay>(NAME_MainBoard, NAME_ULA);

	ScreenSettings.DisplayCycles = SD.DisplayCycles;
	ScreenSettings.Width = ScreenSettings.DisplayCycles.BorderL + ScreenSettings.DisplayCycles.DisplayH + ScreenSettings.DisplayCycles.BorderR;
	ScreenSettings.Height = ScreenSettings.DisplayCycles.BorderT + ScreenSettings.DisplayCycles.DisplayV + ScreenSettings.DisplayCycles.BorderB;
	std::vector<uint32_t> DisplayRGBA(ScreenSettings.Width * ScreenSettings.Height);

	FImageBase& Images = FImageBase::Get();
	ZXColorView->Image = Images.CreateTexture(DisplayRGBA.data(), ScreenSettings.Width, ScreenSettings.Height, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC);
	UI::ConvertZXIndexColorToDisplayRGB(ZXColorView->Image, SD.DisplayData);
	SpectrumDisplay = std::make_shared<FSpectrumDisplay>(SD);

	ZXColorView->UserData = SpectrumDisplay; 
	ZXColorView->Device = Data.Device;
	ZXColorView->DeviceContext = Data.DeviceContext;
	Draw_ZXColorView_Initialize(ZXColorView, UI::ERenderType::Screen);
}

void SScreen::Tick(float DeltaTime)
{
	Status = GetMotherboard().GetState<EThreadStatus>(NAME_MainBoard, NAME_None);
	if (Status == EThreadStatus::Run)
	{
		const FSpectrumDisplay SpectrumDisplayTmp = GetMotherboard().GetState<FSpectrumDisplay>(NAME_MainBoard, NAME_ULA);
		UI::ConvertZXIndexColorToDisplayRGB(ZXColorView->Image, SpectrumDisplayTmp.DisplayData);
	}
}

void SScreen::Render()
{
	if (!IsOpen())
	{
		Close();
		return;
	}

	ImGui::Begin(GetWindowName().c_str(), &bOpen);
	{
		Input_HotKeys();
		Input_Mouse();
		Draw_Display();

		ImGui::End();
	}
}

void SScreen::Destroy()
{
	UI::Draw_ZXColorView_Shutdown(ZXColorView);
}

FMotherboard& SScreen::GetMotherboard() const
{
	return *FAppFramework::Get<FAppDebugger>().Motherboard;
}

void SScreen::Load_MemoryScreen()
{
	const FSpectrumDisplay SpectrumDisplayNew = GetMotherboard().GetState<FSpectrumDisplay>(NAME_MainBoard, NAME_ULA);
	*SpectrumDisplay = SpectrumDisplayNew;
	UI::ConvertZXIndexColorToDisplayRGB(ZXColorView->Image, SpectrumDisplay->DisplayData);
}

void SScreen::Draw_Display()
{
	UI::Draw_ZXColorView(ZXColorView);
}

void SScreen::Input_HotKeys()
{
}

void SScreen::Input_Mouse()
{
	bDragging = false;

	ImGuiContext& Context = *GImGui;
	const float MouseWheel = ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, ImGuiKeyOwner_NoOwner) ? Context.IO.MouseWheel : 0.0f;

	ImGuiWindow* Window = Context.WheelingWindow ? Context.WheelingWindow : Context.HoveredWindow;
	if (!Window || Window->Collapsed || Window->ID != ImGui::GetCurrentWindow()->ID)
	{
		return;
	}

	if (MouseWheel != 0.0f && Context.IO.KeyCtrl && !Context.IO.FontAllowUserScaling)
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

void SScreen::OnInputStep(FCPU_StepType Type)
{
	Load_MemoryScreen();
}

void SScreen::OnInputDebugger(bool bDebuggerState)
{
	ZXColorView->bBeamEnable = bDebuggerState /*true = enter debugger*/;
	if (bDebuggerState)
	{
		Load_MemoryScreen();
	}
}
