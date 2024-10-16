#include "AppDebugger.h"
#include "Fonts/Dos2000_ru_en.cpp"

#include "UI/Viewer.h"
#include "Devices/Device.h"
#include "Devices/CPU/Z80.h"
#include "Devices/ControlUnit/AccessToROM.h"
#include "Devices/Memory/EPROM.h"
#include "Motherboard/Motherboard.h"
#include "Motherboard/Motherboard_Board.h"
#include "Core/Fonts.h"
#include "Core/Image.h"

namespace
{
	static const FName MainBoardName = "Main board";
}

FAppDebugger::FAppDebugger()
{
}

void FAppDebugger::Initialize()
{
	FAppFramework::Initialize();

	LOG("Initialize.");

	FImageBase& Images = FImageBase::Get();
	Images.Initialize(Device);

	Viewer = std::make_shared<SViewer>(NAME_DOS_12, WindowWidth, WindowHeight);
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
		Motherboard->AddBoard(MainBoardName, EName::MainBoard,
		{
			std::make_shared<FCPU_Z80>(),
			std::make_shared<FAccessToROM>(),
			std::make_shared<FEPROM>(EEPROM_Type::EPROM_27C128, 0, std::vector<uint8_t>({ 
				0x00,
				0x01, 0x002, 0x03,
				0x02,
				0x03,
				0x04,
				0x05,
				0x06, 0x55,
				0x07,
				0x08,
				0x09,
				0x0a,
				0x37,
				0x18, 0xef
				}), ESignalState::Low),
		}, 3.5_MHz);

		// load ROM
		//std::filesystem::path FIlePath = "D:\\Work\\Learning\\Emulator\\Rom\\pentagon.rom";// std::filesystem::current_path();
		//Motherboard->LoadRawData(NAME_MainBoard, NAME_EPROM, FIlePath);

		Motherboard->Reset();
		Motherboard->Inut_Debugger();
	}

	FFonts& Fonts = FFonts::Get();
	Fonts.LoadFont(NAME_DOS_12, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 12.0f, 0);
	Fonts.LoadFont(NAME_DOS_14, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 14.0f, 0);
	Fonts.LoadFont(NAME_DISASSEMBLER_16, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 16.0f, 0);
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

	LOG("Shutdown.");

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
	ImGui::PushFont(FFonts::Get().GetFont(NAME_DOS_12));

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
