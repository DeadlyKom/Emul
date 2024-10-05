#pragma once

#include <CoreMinimal.h>
#include "Fonts.h"

class SWindow;

struct FNativeDataInitialize
{
	std::shared_ptr<SWindow> Parent = nullptr;
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
};

struct FWindowInitializer
{
	bool bOpen = true;
	std::string Name;
	EFont::Type FontName;
	bool bIncludeInWindows = false;
	uint32_t Width = -1;
	uint32_t Height = -1;

	FWindowInitializer& SetName(const std::string& _Name)
	{
		Name = _Name;
		return *this;
	}
	FWindowInitializer& SetFontName(EFont::Type _FontName)
	{
		FontName = _FontName;
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
	SWindow(FWindowInitializer& WindowInitializer)
		: bOpen(WindowInitializer.bOpen)
		, bIncludeInWindows(WindowInitializer.bIncludeInWindows)
		, DefaultWidth(WindowInitializer.Width)
		, DefaultHeight(WindowInitializer.Height)
		, Name(WindowInitializer.Name)
		, FontName(WindowInitializer.FontName)
	{}

	virtual void NativeInitialize(const FNativeDataInitialize& _Data)
	{
		Data = _Data;
		Initialize();
	}
	virtual void Initialize() {}
	virtual void Render() {}
	virtual void Tick(float DeltaTime) {}
	virtual void Update() {}
	virtual void Destroy() {}

	virtual void SetOpen(bool _bOpen) { bOpen = _bOpen; }
	virtual void Open() { bOpen = true; }
	virtual void Close() { bOpen = false; }
	virtual bool IsOpen() const { return bOpen; }
	virtual bool IsIncludeInWindows() const { return bIncludeInWindows; }
	virtual const std::string& GetName() const { return Name; }

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

protected:
	FNativeDataInitialize Data;

	bool bOpen;
	bool bIncludeInWindows;

	uint32_t DefaultWidth;
	uint32_t DefaultHeight;
	
	std::string Name;
	EFont::Type FontName;
};
