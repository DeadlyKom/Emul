#include "ViewerBase.h"
#include <Window/Sprite/Canvas.h>

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
	, bDockspaceInitialized(false)
{}

void SViewerBase::Initialize(const std::any& Arg)
{
	if (Arg.type() != typeid(FDockSlot))
	{
		return;
	}

	Layout = std::any_cast<FDockSlot>(Arg);
}

void SViewerBase::Render()
{
	if (Input_HotKeys)
	{
		Input_HotKeys();
	}

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::Begin(GetWindowName().c_str(), &bOpen, window_flags);
	{
		ImGui::PopStyleVar(2);
		if (!bDockspaceInitialized)
		{
			DockspaceID = ImGui::GetID("##DockSpace");
			ImGui::DockBuilderRemoveNode(DockspaceID);
			ImGui::DockBuilderAddNode(DockspaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(DockspaceID, viewport->Size);
			CreateDockLayout(DockspaceID, Layout);
			ImGui::DockBuilderFinish(DockspaceID);
			bDockspaceInitialized = true;
		}
		ImGui::DockSpace(DockspaceID, ImVec2(0, 0), ImGuiDockNodeFlags_AutoHideTabBar);

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
		ImGui::End();
	}

	for (auto& Window : Windows | std::views::values | std::views::join)
	{
		Window->Render();
		if (Window->NeedDock())
		{
			Window->ResetDock();
			ImGui::DockBuilderDockWindow(Window->GetWindowName().c_str(), GetDockID(Window->GetDockSlot()));
		}
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

bool SViewerBase::SetWindowVisibility(EName::Type WindowType, bool bVisibility)
{
	const auto It = Windows.find(WindowType);
	if (It == Windows.end())
	{
		return false;
	}

	for (std::shared_ptr<SWindow>& Window : It->second)
	{
		Window->SetOpen(bVisibility);
	}

	return true;
}

std::vector<std::shared_ptr<SWindow>> SViewerBase::GetWindows(EName::Type WindowType) const
{
	const auto It = Windows.find(WindowType);
	if (It == Windows.end())
	{
		return {};
	}

	return It->second;
}

bool SViewerBase::IsWindowVisibility(EName::Type WindowType) const
{
	const auto It = Windows.find(WindowType);
	if (It == Windows.end())
	{
		return false;
	}

	for (const std::shared_ptr<SWindow>& Window : It->second)
	{
		if (Window->IsOpen())
		{
			return true;
		}
	}

	return false;
}

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
	_Window->SetupHotKeys();

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
		Value->SetupHotKeys();
	}
}

void SViewerBase::RemoveWindow(EName::Type WindowType, std::shared_ptr<SWindow> _Window)
{
	const auto It = Windows.find(WindowType);
	if (It == Windows.end())
	{
		return;
	}

	const auto Window = std::find_if(It->second.begin(), It->second.end(),
		[=](const std::shared_ptr<SWindow>& Ptr)
		{
			return Ptr == _Window;
		});

	if (Window == It->second.end())
	{
		return;
	}

	_Window->DestroyWindow();
}

void SViewerBase::SetMenuBar(MenuBarHandler Handler)
{
	Show_MenuBar = Handler;
}

void SViewerBase::ReleaseUnnecessaryWindows()
{
	for (auto It = Windows.begin(); It != Windows.end();)
	{
		It->second.erase(
			std::remove_if(It->second.begin(), It->second.end(),
				[](const std::shared_ptr<SWindow>& Window)
				{
					const bool bDestroy = Window->IsDestroyWindow();
					if (bDestroy)
					{
						Window->Destroy();
						//Window->ResetWindow();
					}
					return bDestroy;
				}),
			It->second.end()
		);

		if (It->second.empty())
		{
			It = Windows.erase(It);
		}
		else
		{
			++It;
		}
	}
}

bool SViewerBase::IsExistsWindow(EName::Type WindowType, std::shared_ptr<SWindow> Window) const
{
	const auto It = Windows.find(WindowType);
	return It != Windows.end() ? std::any_of(It->second.begin(), It->second.end(),
		[=](const std::shared_ptr<SWindow>& Ptr)
		{
			return Ptr == Window;
		}) : false;
}

void SViewerBase::CreateDockLayout(ImGuiID ParentDockID, FDockSlot& Slot)
{
	if (Slot.SplitDir != ImGuiDir_None)
	{
		const int32_t AtDir = (Slot.SplitDir == ImGuiDir_Left || Slot.SplitDir == ImGuiDir_Up) ? 0 : 1;
		const int32_t OppositeDir = (Slot.SplitDir == ImGuiDir_Left || Slot.SplitDir == ImGuiDir_Up) ? 1 : 0;
		Slot.ChildDockID[AtDir] = ImGui::DockBuilderSplitNode(
			ParentDockID,
			Slot.SplitDir,
			Slot.SplitRatio,
			nullptr,
			&Slot.ChildDockID[OppositeDir]
		);

		if (Slot.ChildSlots.size() > AtDir)
		{
			Slot.ChildSlots[AtDir].ParentDockID = Slot.ChildDockID[AtDir];
		}
		if (Slot.ChildSlots.size() > OppositeDir)
		{
			Slot.ChildSlots[OppositeDir].ParentDockID = Slot.ChildDockID[OppositeDir];
		}
	}
	else
	{
		Slot.ChildDockID[0] = Slot.ChildDockID[1] = ParentDockID;
		if (Slot.ChildSlots.size() > 0)
		{
			Slot.ChildSlots[0].ParentDockID = ParentDockID;
		}
		if (Slot.ChildSlots.size() > 1)
		{
			Slot.ChildSlots[1].ParentDockID = ParentDockID;
		}
	}

	for (FDockSlot& ChildSlot : Slot.ChildSlots)
	{
		CreateDockLayout(ChildSlot.ParentDockID, ChildSlot);
	}
}

ImGuiID SViewerBase::GetDockID(const std::string& SlotName)
{
	std::function<ImGuiID(const std::string&, const FDockSlot&)> RecursiveLambda;
	RecursiveLambda = [&RecursiveLambda](const std::string& _SlotName, const FDockSlot& Slot)->ImGuiID
		{
			if (Slot.SlotName.compare(_SlotName) == 0)
			{
				const int32_t AtDir = (Slot.SplitDir == ImGuiDir_Left || Slot.SplitDir == ImGuiDir_Up) ? 0 : 1;
				return Slot.ChildDockID[AtDir];
			}

			for (const FDockSlot& ChildSlot : Slot.ChildSlots)
			{
				const ImGuiID ResultID = RecursiveLambda(_SlotName, ChildSlot);
				if (ResultID != 0)
				{
					return ResultID;
				}
			}

			return 0;
		};

	return RecursiveLambda(SlotName, Layout);
}
