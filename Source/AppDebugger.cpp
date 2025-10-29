#include "AppDebugger.h"
#include "Fonts/Dos2000_ru_en.cpp"

#include "Window/Debugger/Definition.h"
#include "Window/Debugger/Viewer.h"
#include "Devices/Device.h"
#include "Devices/CPU/Z80.h"
#include "Devices/ControlUnit/ULA.h"
#include "Devices/ControlUnit/AccessToROM.h"
#include "Devices/Memory/EPROM.h"
#include "Devices/Memory/DRAM.h"
#include "Motherboard/Motherboard.h"
#include "Motherboard/Motherboard_Board.h"
#include "Core/Fonts.h"
#include <Core/Image.h>

namespace
{
	static const FName MainBoardName = "Main board";
	static const std::string DebuggerName = std::format(TEXT("ZX-Debugger ver. {}.{}"), DEBUGGER_BUILD_MAJOR, DEBUGGER_BUILD_MINOR);
}

FAppDebugger::FAppDebugger()
{
	WindowName = DebuggerName.c_str();
}

void FAppDebugger::Initialize()
{
	Utils::Log(LogVerbosity::Display, DebuggerName.c_str());

	FAppFramework::Initialize();
	LOG("Initialize 'Debugger' application.");

	LoadIniSettings();

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
				0x18, 0x00,
				0x19,
				0x1a,
				0x1b,
				0x1c,
				0x1d,
				0x1e, 0xfa,
				0x1f,
				0x20, 0x00,
				0x21, 0x20, 0xfd,
				0x22, 0x28, 0x00,
				0x23,
				0x24,
				0x25,
				0x26, 0x45,
				0x27,
				0x28, 0xFD,
				0x29,
				0x2a, 0x01, 0x00,
				0x2b,
				0x2c,
				0x2d,
				0x2e, 0x66,
				0x2f,
				0x30, 0x00,
				0x31, 0x00, 0x00,
				0x32, 0x00, 0x00,
				0x33,
				0x34,
				0x37,
				0x18, 0xaf
				}), ESignalState::Low),
			std::make_shared<FDRAM>(EDRAM_Type::DRAM_4116, 0x4000, std::vector<uint8_t>({1,2,3,4,5,6,7,8})),
		}, 7.0_MHz);

		// load ROM
		//std::filesystem::path FIlePath = "D:\\Work\\Learning\\Emulator\\Rom\\pentagon.rom";// std::filesystem::current_path();
		//Motherboard->LoadRawData(NAME_MainBoard, NAME_EPROM, FIlePath);

		std::filesystem::path FIlePath = "D:\\Work\\[Sprite]\\Sprites\\Raven my.scr";
		//std::filesystem::path FIlePath = "D:\\Work\\[Sprite]\\Sprites\\Menu\\Change Mission\\interact - 7.scr";
		//std::filesystem::path FIlePath = "D:\\Work\\Learning\\Emulator\\Rom\\pentagon.rom";
		Motherboard->LoadRawData(NAME_MainBoard, NAME_DRAM, FIlePath);

		Motherboard->Reset();
	}

	Viewer = std::make_shared<SViewer>(NAME_DOS_12, WindowWidth, WindowHeight);
	if (Viewer)
	{
		FNativeDataInitialize Data;
		Data.Device = Device;
		Data.DeviceContext = DeviceContext;
		Viewer->NativeInitialize(Data);
		Viewer->Initialize();
		Viewer->Inut_Debugger();
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

	FFonts::Get().Reset();
	LOG("Shutdown 'Debugger' application.");
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

void FAppDebugger::LoadIniSettings()
{
	const char* DefaultIni = R"(
[Window][Screen]
Pos=397,221
Size=611,419
Collapsed=0
DockId=0x00000005,0

[Window][CPU State]
Pos=0,18
Size=395,112
Collapsed=0
DockId=0x00000007,0

[Window][Call stack]
Pos=397,642
Size=611,87
Collapsed=0
DockId=0x00000006,0

[Window][Memory dump]
Pos=397,18
Size=611,201
Collapsed=0
DockId=0x00000003,1

[Window][Disassembler]
Pos=0,132
Size=395,597
Collapsed=0
DockId=0x00000008,0

[Window][Signal]
ViewportPos=153,176
ViewportId=0x73A090C3
Size=1631,522
Collapsed=0

[Window][Oscillograph]
Pos=397,18
Size=611,201
Collapsed=0
DockId=0x00000003,0

[Window][WindowOverViewport_11111111]
Pos=0,18
Size=1008,711
Collapsed=0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Docking][Data]
DockSpace       ID=0x7C6B3D9B Window=0xA87D555D Pos=456,205 Size=1008,711 Split=X
  DockNode      ID=0x00000001 Parent=0x7C6B3D9B SizeRef=395,711 Split=Y Selected=0x64548C85
    DockNode    ID=0x00000007 Parent=0x00000001 SizeRef=501,112 HiddenTabBar=1 Selected=0x521B597B
    DockNode    ID=0x00000008 Parent=0x00000001 SizeRef=501,597 HiddenTabBar=1 Selected=0x64548C85
  DockNode      ID=0x00000002 Parent=0x7C6B3D9B SizeRef=611,711 Split=Y
    DockNode    ID=0x00000003 Parent=0x00000002 SizeRef=505,201 Selected=0x84FABE57
    DockNode    ID=0x00000004 Parent=0x00000002 SizeRef=505,508 Split=Y
      DockNode  ID=0x00000005 Parent=0x00000004 SizeRef=505,419 CentralNode=1 HiddenTabBar=1 Selected=0x60967B14
      DockNode  ID=0x00000006 Parent=0x00000004 SizeRef=505,87 HiddenTabBar=1 Selected=0x03384A57

)";
	ImGui::LoadIniSettingsFromMemory(DefaultIni);
}
