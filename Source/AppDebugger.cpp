#include "AppDebugger.h"
#include "Fonts/Dos2000_ru_en.cpp"
#include <Devices/Device_CPU_Z80.h>

namespace
{
	static const FName FontName_Dos2000 = "Dos2000";
}

FAppDebugger::FAppDebugger()
{
}

void FAppDebugger::Initialize()
{
	FAppFramework::Initialize();

	std::cout << "Debugger: Initialize." << std::endl;

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
		std::vector<std::shared_ptr<FDevice>> Devices =
		{ 
			std::make_shared<FCPU_Z80>(),
		};
		Motherboard->Initialize(Devices);
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

	std::cout << "Debugger: Shutdown." << std::endl;

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

