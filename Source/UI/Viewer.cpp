#include "Viewer.h"

#include "UI/CPU_State.h"
#include "UI/Disassembler.h"

namespace
{
	static const char* ViewerName = TEXT("Viewer");
	static const char* MenuFileName = TEXT("File");
	static const char* MenuWindowsName = TEXT("Windows");
}

SViewer::SViewer(uint32_t _Width, uint32_t _Height)
	: SWindow(ViewerName, false, _Width, _Height)
{}

void SViewer::NativeInitialize(const FNativeDataInitialize& _Data)
{
	SWindow::NativeInitialize(_Data);

	Windows = { { EWindowsType::CPU_State,			std::make_shared<SCPU_State>()			},
				{ EWindowsType::Disassembler,		std::make_shared<SDisassembler>()		},
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

	if (ImGui::BeginMainMenuBar())
	{
		ShowMenuFile();
		ShowWindows();

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

void SViewer::ShowMenuFile()
{
	if (ImGui::BeginMenu(MenuFileName))
	{
		ImGui::EndMenu();
	}
}

void SViewer::ShowWindows()
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
