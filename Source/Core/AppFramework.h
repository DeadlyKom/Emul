#pragma once

#include <CoreMinimal.h>
#include "Fonts.h"
#include "SystemTime.h"

class FAppFramework
{
public:
	FAppFramework();
	virtual ~FAppFramework();

	template<typename T>
	static T& Get()
	{
		static std::shared_ptr<T> InstanceAppFramework(new T());
		return *InstanceAppFramework.get();
	}

	int32_t Launch(const std::map<std::string, std::string>& Args);

	// virtual methods
	virtual bool Startup(const std::map<std::string, std::string>& Args);
	virtual void Initialize();
	virtual void Shutdown();
	virtual void Tick(float DeltaTime);
	virtual void Render();
	virtual bool IsOver();
	virtual void SetRectWindow(uint16_t Width, uint16_t Height);
	virtual std::vector<std::wstring> DragAndDropExtensions() const { return {}; }
	virtual void DragAndDropFile(const std::filesystem::path& FilePath) {}

protected:
	// internal variables
	uint32_t ScreenWidth;
	uint32_t ScreenHeight;
	uint32_t WindowWidth;
	uint32_t WindowHeight;

	FFonts Fonts;
	FSystemTime Time;

	std::string ClassName;
	std::string WindowName;
	std::string IniFilePath;
	std::string LogFilePath;

	bool bEnabledResize;
	ID3D11Device* Device;
	ID3D11DeviceContext* DeviceContext;

private:
	// internal functions
	void Release();
	void Register();
	bool Create(int32_t Width, int32_t Height);
	int32_t Run();
	void Idle();

	bool CreateDeviceD3D();
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();

	bool InitField();
	void SaveDefaultImGuiIni();
	bool StartupGUI();
	void ShutdownGUI();

	HINSTANCE hInstance;
	ATOM AtomClass;
	HWND hwndAppFramework;
	RECT RectWindow;

	IDXGISwapChain* SwapChain;
	ID3D11RenderTargetView* RenderTargetView;

	ImVec4 BackgroundColor;
};
