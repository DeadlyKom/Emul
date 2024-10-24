#include "Viewer.h"

#include "AppDebugger.h"
#include "UI/Screen.h"
#include "UI/CallStack.h"
#include "UI/CPU_State.h"
#include "UI/MemoryDump.h"
#include "UI/Disassembler.h"
#include "Utils/Hotkey.h"
#include "Motherboard/Motherboard.h"

namespace
{
	static const char* ThisWindowName = TEXT("Viewer");
	static const char* MenuFileName = TEXT("File");
	static const char* MenuEmulationName = TEXT("Emulation");
	static const char* MenuWindowsName = TEXT("Windows");
}

SViewer::SViewer(EFont::Type _FontName, uint32_t _Width, uint32_t _Height)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(FontName)
		.SetIncludeInWindows(false)
		.SetWidth(_Width)
		.SetHeight(_Height))
{}

void SViewer::NativeInitialize(const FNativeDataInitialize& _Data)
{
	SWindow::NativeInitialize(_Data);

	Windows = { 
				{ EWindowsType::Screen,				std::make_shared<SScreen>(NAME_DOS_12)					},
				{ EWindowsType::CallStack,			std::make_shared<SCallStack>(NAME_DOS_12)				},
				{ EWindowsType::CPU_State,			std::make_shared<SCPU_State>(NAME_DOS_14)				},
				{ EWindowsType::MemoryDump,			std::make_shared<SMemoryDump>(NAME_MEMORY_DUMP_16)		},
				{ EWindowsType::Disassembler,		std::make_shared<SDisassembler>(NAME_DISASSEMBLER_16)	},
			  };

	// initialize windows
	{
		FNativeDataInitialize Data = _Data;
		Data.Parent = shared_from_this();

		for (auto& [Type, Window] : Windows)
		{
			Window->NativeInitialize(Data);
		}
	}
}

void SViewer::Initialize()
{
}

void SViewer::Render()
{
	ImGui::DockSpaceOverViewport();

	Input_HotKeys();

	if (ImGui::BeginMainMenuBar())
	{
		ShowMenu_File();
		ShowMenu_Emulation();
		ShowMenu_Windows();

		ImGui::EndMainMenuBar();
	}
}

void SViewer::Tick(float DeltaTime)
{
	for (auto& [Type, Window] : Windows)
	{
		Window->Tick(DeltaTime);
	}
}

void SViewer::Destroy()
{
	for (auto& [Type, Window] : Windows)
	{
		Window->Destroy();
	}
}

void SViewer::Input_HotKeys()
{
	static std::vector<FHotKey> Hotkeys =
	{
		{ ImGuiKey_GraveAccent,			ImGuiInputFlags_None,	std::bind(&ThisClass::Inut_Debugger, this)			 },		// debugger
		{ ImGuiKey_F11,					ImGuiInputFlags_None,	[this]() { GetMotherboard().NonmaskableInterrupt(); }},		// NMI
		{ ImGuiMod_Ctrl | ImGuiKey_F12, ImGuiInputFlags_None,	[this]() { GetMotherboard().Reset();				}},		// Reset
	};

	HotKey::Handler(Hotkeys);
}

void SViewer::Inut_Debugger()
{
	FMotherboard& Motherboard = GetMotherboard();
	Motherboard.Inut_Debugger();
	const bool bDebuggerState = Motherboard.GetDebuggerState();
	SendEventNotification(EEventNotificationType::Input_Debugger, bDebuggerState);
}

FMotherboard& SViewer::GetMotherboard() const
{
	return *FAppFramework::Get<FAppDebugger>().Motherboard;
}

void SViewer::ShowMenu_File()
{
	if (ImGui::BeginMenu(MenuFileName))
	{
		ImGui::EndMenu();
	}
}

void SViewer::ShowMenu_Emulation()
{
	if (ImGui::BeginMenu(MenuEmulationName))
	{
		if (ImGui::MenuItem("ToDo"))
		{
			int a = 10;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Reset", "F12"))
		{
			GetMotherboard().Reset();
		}
		if (ImGui::MenuItem("NMI", "F11"))
		{
			GetMotherboard().NonmaskableInterrupt();
		}
		ImGui::EndMenu();
	}
}

void SViewer::ShowMenu_Windows()
{
	if (ImGui::BeginMenu(MenuWindowsName))
	{
		for (auto& [Type, Window] : Windows)
		{
			if (Window->IsIncludeInWindows())
			{
				if (ImGui::MenuItem(Window->GetName().c_str(), 0, Window->IsOpen()))
				{
					if (Window->IsOpen())
					{
						Window->Close();
					}
					else
					{
						Window->Open();
					}
				}
			}
		}

		ImGui::EndMenu();
	}

	for (auto& [Type, Window] : Windows)
	{
		Window->Render();
	}
}
