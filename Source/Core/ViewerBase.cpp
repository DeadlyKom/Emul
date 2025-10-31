#include "ViewerBase.h"

namespace
{
	static const wchar_t* ThisWindowName = L"ViewerBase";
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

void SViewerBase::AddWindow(EName::Type WindowType, std::shared_ptr<SWindow> _Window, const FNativeDataInitialize& _Data, const std::any& Arg)
{
	if (IsExistsWindow(WindowType, _Window))
	{
		return;
	}

	FNativeDataInitialize Data = _Data;
	Data.Parent = shared_from_this();

	_Window->NativeInitialize(Data);
	_Window->Initialize(Arg);

	Windows[WindowType].push_back(_Window);
}

void SViewerBase::AppendWindows(const std::map<EName::Type, std::shared_ptr<SWindow>>& _Windows, const FNativeDataInitialize& _Data, const std::any& Arg)
{
	FNativeDataInitialize Data = _Data;
	Data.Parent = shared_from_this();

	for (const auto& [Key, Value] : _Windows)
	{	
		if (IsExistsWindow(Key, Value))
		{
			continue;
		}
		Windows[Key].push_back(Value);

		Value->NativeInitialize(Data);
		Value->Initialize(Arg);
	}
}

void SViewerBase::SetMenuBar(MenuBarHandler Handler)
{
	Show_MenuBar = Handler;
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
			for (auto& [Key, WindowList] : Windows)
			{
				if (Key == EName::Canvas)
				{
					continue;
				}

				for (auto& Window : WindowList)
				{
					if (!Window->IsIncludeInWindows())
					{
						continue;
					}

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

		ImGui::EndMainMenuBar();
	}

	for (auto& Window : Windows | std::views::values | std::views::join)
	{
		Window->Render();
	}
}

void SViewerBase::Tick(float DeltaTime)
{
	for (auto& Window : Windows | std::views::values | std::views::join)
	{
		Window->Tick(DeltaTime);
	}
}

void SViewerBase::Destroy()
{
	for (auto& Window : Windows | std::views::values | std::views::join)
	{
		Window->Destroy();
	}
}

bool SViewerBase::IsExistsWindow(EName::Type WindowType, std::shared_ptr<SWindow> Window) const
{
	const auto It = Windows.find(WindowType);
	return It != Windows.end() ? std::any_of(It->second.begin(), It->second.end(),
		[=](const std::shared_ptr<SWindow>& ptr)
		{
			return ptr == Window;
		}) : false;
}
