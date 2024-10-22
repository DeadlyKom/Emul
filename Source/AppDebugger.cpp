#include "AppDebugger.h"
#include "Fonts/Dos2000_ru_en.cpp"

#include "UI/Viewer.h"
#include "Devices/Device.h"
#include "Devices/CPU/Z80.h"
#include "Devices/ControlUnit/ULA.h"
#include "Devices/ControlUnit/AccessToROM.h"
#include "Devices/Memory/EPROM.h"
#include "Devices/Memory/DRAM.h"
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
	Images.Initialize(Device, DeviceContext);

	Motherboard = std::make_shared<FMotherboard>();
	if (Motherboard)
	{
		Motherboard->Initialize();
		Motherboard->AddBoard(MainBoardName, EName::MainBoard,
		{
			std::make_shared<FCPU_Z80>(3.5_MHz),
			std::make_shared<FULA>(FDisplayCycles{
				/*FlybackH*/96, /*BorderL*/32, /*DisplayH*/256, /*BorderR*/64,
				/*FlybackV*/8, /*BorderT*/56, /*DisplayV*/192, /*BorderB*/56}, 7.0_MHz),
			std::make_shared<FAccessToROM>(),
			std::make_shared<FEPROM>(EEPROM_Type::EPROM_27C128, 0x0000, std::vector<uint8_t>({ 
				0x00,
				0x01, 0x002, 0x03,
				0x02,
				0x03,
				0x04,
				0x05,
				0x06, 0x05,
				0x07,
				0x08,
				0x09,
				0x0a,
				0x0b,
				0x0c,
				0x0d,
				0x0e, 0xaa,
				0x0f,
				0x10, 0xfd,
				0x11, 0xdd, 0xee,
				0x12,
				0x13,
				0x14,
				0x15,
				0x16, 0x04,
				0x17,
				//0x18, 0xdd
				0x19,
				0x1a,
				0x1b,
				0x1c,
				0x1d,
				0x1e, 0xfa,
				0x1f,
				0x00, 0x00, // 0x20, 0xfd,
				0x21, 0x20, 0xfd,
				0x22, 0x28, 0x00,
				0x23,
				0x24,
				0x25,
				0x26, 0x45,
				0x37,
				0x18, 0xc8
				}), ESignalState::Low),
			std::make_shared<FDRAM>(EDRAM_Type::DRAM_4116, 0x4000, std::vector<uint8_t>({1,2,3,4,5,6,7,8})),
		}, 7.0_MHz);

		// load ROM
		//std::filesystem::path FIlePath = "D:\\Work\\Learning\\Emulator\\Rom\\pentagon.rom";// std::filesystem::current_path();
		//Motherboard->LoadRawData(NAME_MainBoard, NAME_EPROM, FIlePath);

		std::filesystem::path FIlePath = "D:\\Work\\[Sprite]\\Sprites\\Menu\\Change Mission\\interact - 7.scr";
		//std::filesystem::path FIlePath = "D:\\Work\\Learning\\Emulator\\Rom\\pentagon.rom";
		Motherboard->LoadRawData(NAME_MainBoard, NAME_DRAM, FIlePath);

		Motherboard->Reset();
		//Motherboard->Inut_Debugger();
	}

	Viewer = std::make_shared<SViewer>(NAME_DOS_12, WindowWidth, WindowHeight);
	if (Viewer)
	{
		FNativeDataInitialize Data;
		Data.Device = Device;
		Data.DeviceContext = DeviceContext;
		Viewer->NativeInitialize(Data);
	}

	FFonts& Fonts = FFonts::Get();
	Fonts.LoadFont(NAME_DOS_12, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 12.0f, 0);
	Fonts.LoadFont(NAME_DOS_14, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 14.0f, 0);
	Fonts.LoadFont(NAME_MEMORY_DUMP_16, &Dos2000_ru_en_compressed_data[0], Dos2000_ru_en_compressed_size, 16.0f, 0);
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
