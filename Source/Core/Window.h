#pragma once

#include <locale>
#include <codecvt>
#include <CoreMinimal.h>
#include "Fonts.h"

class SWindow;

struct FNativeDataInitialize
{
	std::weak_ptr<SWindow> Parent;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
};

struct FWindowInitializer
{
	bool bOpen = true;
	std::wstring Name;
	std::string DockSlot;
	EFont::Type FontName;
	bool bIncludeInWindows = false;
	uint32_t Width = -1;
	uint32_t Height = -1;

	FWindowInitializer& SetName(const std::wstring& _Name)
	{
		Name = _Name;
		return *this;
	}
	FWindowInitializer& SetFontName(EFont::Type _FontName)
	{
		FontName = _FontName;
		return *this;
	}
	FWindowInitializer& SetDockSlot(std::string _DockSlot)
	{
		DockSlot = _DockSlot;
		return *this;
	}
	FWindowInitializer& SetIncludeInWindows(bool _bIncludeInWindows)
	{
		bIncludeInWindows = _bIncludeInWindows;
		return *this;
	}
	FWindowInitializer& SetWidth(uint32_t _Width)
	{
		Width = _Width;
		return *this;
	}
	FWindowInitializer& SetHeight(uint32_t _Height)
	{
		Height = _Height;
		return *this;
	}
};

class SWindow : public std::enable_shared_from_this<SWindow>
{
public:
	SWindow(const FWindowInitializer& _WindowInitializer)
		: WindowInitializer(_WindowInitializer)
		, bOpen(_WindowInitializer.bOpen)
		, bIncludeInWindows(_WindowInitializer.bIncludeInWindows)
		, DefaultWidth(_WindowInitializer.Width)
		, DefaultHeight(_WindowInitializer.Height)
		, Name(_WindowInitializer.Name)
		, DockSlot(_WindowInitializer.DockSlot)
		, FontName(_WindowInitializer.FontName)
		, bPendingKill(false)
		, bInitializeWindow(true)
		, TickCounter(0)
	{
		bNeedDock = !DockSlot.empty();
	}
	virtual ~SWindow() = default;

	virtual void NativeInitialize(const FNativeDataInitialize& _Data)
	{
		Data = _Data;
	}
	virtual void Initialize(const std::any& Arg) {}
	virtual void Render()
	{
		bInitializeWindow = ++TickCounter < 3;
	}
	virtual void Tick(float DeltaTime) {}
	virtual void Update() {}
	virtual void Destroy() {}

	virtual void SetOpen(bool _bOpen) { bOpen = _bOpen; }
	virtual void Open() { bOpen = true; }
	virtual void Close() { bOpen = false; }
	virtual bool IsOpen() const { return bOpen; }
	virtual bool IsIncludeInWindows() const { return bIncludeInWindows; }
	virtual void SetWindowDefaultPosSize()
	{
		if (DefaultWidth == -1 || DefaultHeight == -1)
		{
			return;
		}

		ImVec2 WindowPos = ImGui::GetWindowPos();
		ImGui::SetNextWindowPos(ImVec2(WindowPos.x, WindowPos.y), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2((float)DefaultWidth, (float)DefaultHeight), ImGuiCond_FirstUseEver);
	}
	virtual std::string ToString() const
	{
		return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(Name);
	}

	inline std::string GetWindowName() const
	{
		return ToString();
	}
	void DestroyWindow() { bPendingKill = true; }
	bool IsDestroyWindow() const { return bPendingKill; }
	const FNativeDataInitialize& GetNativeDataInitialize() const { return Data; }
	void ResetWindow()
	{
		bOpen = WindowInitializer.bOpen;
		bIncludeInWindows = WindowInitializer.bIncludeInWindows;
		DefaultWidth = WindowInitializer.Width;
		DefaultHeight = WindowInitializer.Height;
		Name = WindowInitializer.Name;
		FontName = WindowInitializer.FontName;
		bPendingKill = false;
		bInitializeWindow = true;
		TickCounter = 0;
	}
	std::string GetDockSlot() const { return DockSlot; }

	bool bNeedDock;
protected:
	FNativeDataInitialize Data;
	FWindowInitializer WindowInitializer;

	bool bOpen;
	bool bPendingKill;
	bool bIncludeInWindows;
	bool bInitializeWindow;

	int32_t TickCounter;
	uint32_t DefaultWidth;
	uint32_t DefaultHeight;

	std::wstring Name;
	std::string DockSlot;
	EFont::Type FontName;
};
