#include "AppDebugger.h"
#include "Fonts/Dos2000_ru_en.cpp"

#include "UI/Viewer.h"
#include "Devices/Device.h"
#include "Devices/Device_CPU_Z80.h"
#include "Motherboard/Motherboard.h"
#include "Motherboard/Motherboard_Board.h"

namespace
{
	static const FName FontName_Dos2000 = "Dos2000";
	static const FName MainBoardName = "Main board";
}

FAppDebugger::FAppDebugger()
{
}

void FAppDebugger::Initialize()
{
	FAppFramework::Initialize();

	LOG_CONSOLE("Initialize.");

	Viewer = std::make_shared<SViewer>(WindowWidth, WindowHeight);
	if (Viewer)
	{
		FNativeDataInitialize Data;
		Data.Device = Device;
		Data.DeviceContext = DeviceContext;
		Viewer->NativeInitialize(Data);
	}

	Motherboard = std::make_shared<FMotherboard>();
	if (Motherboard)
	{
		Motherboard->Initialize();
		Motherboard->AddBoard(MainBoardName,
		{
			std::make_shared<FCPU_Z80>(),
		}, 3.5_MHz);
	}

	FFonts& Fonts = FFonts::Get();
	ImFont* NewFont = Fonts.LoadFont(FontName_Dos2000, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 12.0f, 0);
}

void FAppDebugger::Shutdown()
{
	if (Viewer)
	{
		Viewer->Destroy();
		Viewer.reset();
	}

	if (Motherboard)
	{
		Motherboard->Shutdown();
		Motherboard.reset();
	}

	auto a = Flags.bLog;

	LOG_CONSOLE("Shutdown.");

	FAppFramework::Shutdown();
}

void FAppDebugger::Tick(float DeltaTime)
{
	FAppFramework::Tick(DeltaTime);

	if (Viewer)
	{
		Viewer->Tick(DeltaTime);
	}
}

void FAppDebugger::Render()
{
	ImGui::PushFont(FFonts::Get().GetFont(FontName_Dos2000));

	FAppFramework::Render();

	if (Viewer)
	{
		Viewer->Render();
	}

	ImGui::PopFont();
}

bool FAppDebugger::IsOver()
{
	return Viewer ? !Viewer->IsOpen() : true;
}

