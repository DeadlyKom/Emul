#include "Viewer.h"

#include "AppDebugger.h"
#include "Window/Debugger/Screen.h"
#include "Window/Debugger/CallStack.h"
#include "Window/Debugger/CPU_State.h"
#include "Window/Debugger/MemoryDump.h"
#include "Window/Debugger/Disassembler.h"
#include "Window/Debugger/Oscillograph.h"
#include "Utils/Hotkey.h"
#include "Motherboard/Motherboard.h"

namespace
{
	static const wchar_t* ThisWindowName = L"Viewer";
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
				{ EWindowsType::Oscillograph,		std::make_shared<SOscillograph>(NAME_OSCILLOGRAPH_16)	},
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

void SViewer::Initialize(const std::any& Arg)
{
	for (auto& [Type, Window] : Windows)
	{
		Window->Initialize(Arg);
	}
}

void SViewer::SetupHotKeys()
{
	auto Self = std::dynamic_pointer_cast<SViewer>(shared_from_this());
	Hotkeys =
	{
		{ ImGuiKey_GraveAccent,			ImGuiInputFlags_None,	std::bind(&ThisClass::Inut_Debugger, Self)					},		// debugger
		{ ImGuiKey_F11,					ImGuiInputFlags_None,	[Self]() { Self->GetMotherboard().NonmaskableInterrupt();	}},		// NMI
		{ ImGuiMod_Ctrl | ImGuiKey_F12, ImGuiInputFlags_None,	[Self]() { Self->GetMotherboard().Reset();					}},		// Reset
	};
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
				if (ImGui::MenuItem(Window->GetWindowName().c_str(), 0, Window->IsOpen()))
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
