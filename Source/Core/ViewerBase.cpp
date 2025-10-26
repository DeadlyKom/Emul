#include "ViewerBase.h"

namespace
{
	static const char* ThisWindowName = TEXT("ViewerBase");
	static const char* MenuWindowsName = TEXT("Windows");
}

SViewerBase::SViewerBase(EFont::Type _FontName, uint32_t _Width, uint32_t _Height)
	: Super(FWindowInitializer()
		.SetName(ThisWindowName)
		.SetFontName(FontName)
		.SetIncludeInWindows(false)
		.SetWidth(_Width)
		.SetHeight(_Height))
{}

void SViewerBase::AppendWindows(const std::map<EName::Type, std::shared_ptr<SWindow>>& _Windows)
{
	for (const auto& [key, value] : _Windows)
	{
		Windows[key] = value;
	}
}

void SViewerBase::SetMenuBar(MenuBarHandler Handler)
{
	Show_MenuBar = Handler;
}

void SViewerBase::NativeInitialize(const FNativeDataInitialize& _Data)
{
	Super::NativeInitialize(_Data);

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

void SViewerBase::Render()
{
	ImGui::DockSpaceOverViewport();
	if (Input_HotKeys)
	{
		Input_HotKeys();
	}
	if (ImGui::BeginMainMenuBar())
	{
		if (Show_MenuBar)
		{
			Show_MenuBar();
		}

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

		ImGui::EndMainMenuBar();
	}

	for (auto& [Type, Window] : Windows)
	{
		Window->Render();
	}
}

void SViewerBase::Tick(float DeltaTime)
{
	for (auto& [Type, Window] : Windows)
	{
		Window->Tick(DeltaTime);
	}
}

void SViewerBase::Destroy()
{
	for (auto& [Type, Window] : Windows)
	{
		Window->Destroy();
	}
}
