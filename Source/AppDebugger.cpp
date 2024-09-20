#include "AppDebugger.h"
#include "Fonts/Dos2000_ru_en.cpp"

namespace
{
	static const char* FontName_Dos2000 = "Dos2000";
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

	FFonts& Fonts = FFonts::Get();
	ImFont* NewFont = Fonts.LoadFont(FontName_Dos2000, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 11.0f, 0);
}

void FAppDebugger::Shutdown()
{
	std::cout << "Debugger: Initialize." << std::endl;

	if (Viewer)
	{
		Viewer->Destroy();
		Viewer.reset();
	}

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

