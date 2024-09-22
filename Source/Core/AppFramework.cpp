#include "AppFramework.h"

#include <iostream>
#include "resource.h"
#include "backends\imgui_impl_win32.h"
#include "backends\imgui_impl_dx11.h"

namespace Path
{
	static const char* Log = "Saved/Logs";
	static const char* Config = "Saved/Config";

	static const char* IniFilename = "imgui.ini";
}

FFrameworkFlags FAppFramework::Flags;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LONG_PTR CALLBACK AppFrameworkProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static FAppFramework* AppFramework = nullptr;

	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
	{
		return true;
	}

	switch (msg)
	{
	case WM_CREATE:
		AppFramework = (FAppFramework*)((LPCREATESTRUCT)lParam)->lpCreateParams;
		break;

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			AppFramework->SetRectWindow(LOWORD(lParam), HIWORD(lParam));
		}
		return 0;

	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)			// Disable ALT application menu
		{
			return 0;
		}
		break;

	case WM_DPICHANGED:
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DpiEnableScaleViewports)
		{
			const RECT* SuggestedRECT = (RECT*)lParam;
			SetWindowPos(hWnd, NULL, SuggestedRECT->left, SuggestedRECT->top, SuggestedRECT->right - SuggestedRECT->left, SuggestedRECT->bottom - SuggestedRECT->top, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);;
}

FAppFramework::FAppFramework()
	: WindowWidth(1024)
	, WindowHeight(768)
	, AtomClass(0)
	, Device(nullptr)
	, DeviceContext(nullptr)
	, SwapChain(nullptr)
	, RenderTargetView(nullptr)
	, BackgroundColor(ImVec4(0.0f, 0.0f, 0.0f, 1.0f))
	, ClassName(TEXT("DefaultClassName"))
	, WindowName(Debugger::Name.c_str())
{
	ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
}

FAppFramework::~FAppFramework()
{}


int32_t FAppFramework::Launch(const std::map<std::string, std::string>& Args)
{
	if (Args.contains("help"))
	{
		LOG_CONSOLE("help:\n");
		LOG_CONSOLE("   -log \t\t\t\t enable logging.\n");
		LOG_CONSOLE("   -resolution <width>x<height> \t set the required resolution.\n");
		LOG_CONSOLE("   -fullscreen \t\t\t set resolution to fullscreen.\n");

		LOG_CONSOLE("ToDo: add command list.");

		return 0;
	}

	// startup
	if (!Startup(Args))
	{
		Shutdown();
		return 1;
	}

	LOG_CONSOLE("Launch...");

	Register();

	if (!Create(WindowWidth, WindowHeight))
	{
		Shutdown();
		return 1;
	}

	Initialize();
	if (!StartupGUI())
	{
		Shutdown();
		return 1;
	}

	const int32_t Result = Run();
	Shutdown();

	return Result;
}

bool FAppFramework::Startup(const std::map<std::string, std::string>& Args)
{
	for (const auto& [Key, Value] : Args)
	{
		if (!Key.compare("log"))
		{
			Flags.bLog = true;

			LOG_CONSOLE("[*] enable logging.");
		}
		else if (!Key.compare("resolution"))
		{
			const std::string Delimiter = "x";
			const size_t Pos = Value.find(Delimiter);
			if (Pos != std::string::npos)
			{
				const std::string NewWindowWidth = Value.substr(0, Pos);
				const std::string NewWindowHeight = Value.substr(Pos + 1);

				WindowWidth = atoi(NewWindowWidth.data());
				WindowHeight = atoi(NewWindowHeight.data());
			}

			LOG_CONSOLE("[*] set the resolution {}x{}", WindowWidth, WindowHeight);
		}
		else if (!Key.compare("fullscreen"))
		{
			Flags.bFullscreen = true;

			WindowWidth = ScreenWidth;
			WindowHeight = ScreenHeight;

			LOG_CONSOLE("[*] set resolution to fullscreen.");
		}
	}

	if (!InitField())
	{
		LOG_CONSOLE("Startup fail.");
		return false;
	}

	// setup Dear ImGui context
	if (!IMGUI_CHECKVERSION())
	{
		LOG_CONSOLE("Startup fail.");
		return false;
	}

	ImGuiContext* Context = ImGui::CreateContext();
	//ImGuiIO& IO = ImGui::GetIO();

	//if (Flags.bLog)
	//{

	//	char Buffer[80];
	//	std::time_t Time = std::time(nullptr);
	//	std::tm* Now = std::localtime(&Time);

	//	if (true)
	//	{
	//		strftime(Buffer, sizeof(Buffer), "%d.%m.%Y-%H.%M.%S", Now);
	//		LogFilePath = std::format("{}/{}.log", Path::Log, Buffer);
	//		IO.LogFilename = LogFilePath.c_str();
	//	}

	//	ImGui::LogToFile();
	//	strftime(Buffer, sizeof(Buffer), "Log file open, %Y/%m/%d %X", Now);
	//	ImGui::LogText(IM_NEWLINE);
	//	LOG(Buffer);
	//}

	////Context->LogEnabled = Flags.bLog; 
	//LOG("Framework: Startup fail");

	return true;
}

void FAppFramework::Initialize()
{
	LOG_CONSOLE("Initialize.");
	Time.Initialize();
}

void FAppFramework::Shutdown()
{
	ShutdownGUI();
	DestroyWindow(hwndAppFramework);
	Release();

	LOG_CONSOLE("Shutdown.");
}

void FAppFramework::Tick(float DeltaTime)
{
}

void FAppFramework::Render()
{
}

bool FAppFramework::IsOver()
{
	return false;
}

void FAppFramework::SetRectWindow(uint16_t Width, uint16_t Height)
{
	if (Device != nullptr)
	{
		CleanupRenderTarget();
		SwapChain->ResizeBuffers(0, (UINT)Width, (UINT)Height, DXGI_FORMAT_UNKNOWN, 0);
		CreateRenderTarget();
	}
}

void FAppFramework::Release()
{
	CleanupDeviceD3D();

	if (AtomClass != 0)
	{
		UnregisterClass((LPCSTR)AtomClass, hInstance);
	}
}

void FAppFramework::Register()
{
	LOG_CONSOLE("Register.");

	WNDCLASSEX wcex;
	hInstance = GetModuleHandle(nullptr);

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_CLASSDC/* | CS_HREDRAW | CS_VREDRAW*/;
	wcex.lpfnWndProc = (WNDPROC)AppFrameworkProc;
	wcex.cbClsExtra =
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOW));
	wcex.hCursor = nullptr;
	wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = ClassName.c_str();
	wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL));

	AtomClass = RegisterClassEx(&wcex);
}

bool FAppFramework::Create(int32_t Width, int32_t Height)
{
	LOG_CONSOLE("Create windows.");

	const uint32_t WindowWidth = Width > 0 ? Width : ScreenWidth >> 1;
	const uint32_t WindowHeight = Height > 0 ? Height : ScreenHeight >> 1;
	const uint32_t WindowPositionX = (ScreenWidth - WindowWidth) >> 1;
	const uint32_t WindowPositionY = (ScreenHeight - WindowHeight) >> 1;

	hwndAppFramework = CreateWindowEx(NULL,
		(LPCSTR)AtomClass,
		WindowName.c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		WindowPositionX, WindowPositionY,
		WindowWidth, WindowHeight,
		nullptr,
		nullptr,
		hInstance,
		this);

	if (!CreateDeviceD3D())
	{
		LOG_CONSOLE("Create windows failed.");
		return false;
	}

	UpdateWindow(hwndAppFramework);
	GetClientRect(hwndAppFramework, &RectWindow);

	LOG_CONSOLE("Create windows success.");
	return true;
}

int32_t FAppFramework::Run()
{
	LOG_CONSOLE("Run.");

	MSG msg;
	bool bRun = true;

	while (bRun)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_NOREMOVE))
		{
			if (GetMessage(&msg, nullptr, 0, 0))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				bRun = false;
				return (int32_t)msg.wParam;;
			}
		}

		if (bRun)
		{
			Idle();
		}

		bRun = !IsOver();
	}

	return (int32_t)msg.wParam;
}

void FAppFramework::Idle()
{
	static int64_t OldTime = Time.GetCurrentTick();

	const int64_t CurrentTime = Time.GetCurrentTick();
	const float DeltaTime = (float)Time.TimeBetweenTicks(OldTime, CurrentTime);
	OldTime = CurrentTime;

	Tick(DeltaTime);

	// prepare ImGui frame
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	Render();

	//ImGui::ShowMetricsWindow();
	//ImGui::ShowDebugLogWindow();
	//ImGui::ShowStackToolWindow();

	// rendering ImGui frame
	{
		ImGui::Render();
		const float clear_color_with_alpha[4] = { BackgroundColor.x * BackgroundColor.w, BackgroundColor.y * BackgroundColor.w, BackgroundColor.z * BackgroundColor.w, BackgroundColor.w };
		DeviceContext->OMSetRenderTargets(1, &RenderTargetView, nullptr);
		DeviceContext->ClearRenderTargetView(RenderTargetView, clear_color_with_alpha);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// update and Render additional Platform Windows
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
		SwapChain->Present(!!Flags.bVsync, 0);
	}
}

bool FAppFramework::CreateDeviceD3D()
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hwndAppFramework;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT CreateDeviceFlags = 0;
	//CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL FeatureLevel;
	const D3D_FEATURE_LEVEL FeatureLevelArray[3] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL, CreateDeviceFlags, FeatureLevelArray, 3, D3D11_SDK_VERSION, &sd, &SwapChain, &Device, &FeatureLevel, &DeviceContext)))
	{
		LOG_CONSOLE("Create device and swap chain.");
		return false;
	}

	CreateRenderTarget();
	return true;
}

void FAppFramework::CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (SwapChain != nullptr)
	{
		SwapChain->Release();
		SwapChain = nullptr;
	}
	if (DeviceContext != nullptr)
	{
		DeviceContext->Release();
		DeviceContext = nullptr;
	}
	if (Device != nullptr)
	{
		Device->Release();
		Device = nullptr;
	}
}

void FAppFramework::CreateRenderTarget()
{
	ID3D11Texture2D* BackBuffer;
	SwapChain->GetBuffer(0, IID_PPV_ARGS(&BackBuffer));
	if (BackBuffer != nullptr)
	{
		Device->CreateRenderTargetView(BackBuffer, nullptr, &RenderTargetView);
		BackBuffer->Release();
	}
}

void FAppFramework::CleanupRenderTarget()
{
	if (RenderTargetView != nullptr)
	{
		RenderTargetView->Release();
		RenderTargetView = nullptr;
	}
}

bool FAppFramework::InitField()
{
	IniFilePath = std::format(TEXT("{}/{}"), Path::Config, Path::IniFilename);

	if (!std::filesystem::exists(Path::Log))
	{
		if (!std::filesystem::create_directories(Path::Log))
		{
			return false;
		}
	}

	if (!std::filesystem::exists(Path::Config))
	{
		if (!std::filesystem::create_directories(Path::Config))
		{
			return false;
		}
	}

	if (!std::filesystem::exists(IniFilePath.c_str()))
	{
		SaveDefaultImGuiIni();
	}

	return true;
}

void FAppFramework::SaveDefaultImGuiIni()
{
}

bool FAppFramework::StartupGUI()
{
	LOG_CONSOLE("Startup ImGui.");

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
	IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	ImGuiStyle& Style = ImGui::GetStyle();

	// when viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
	if (IO.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		Style.WindowRounding = 0.0f;
		Style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	//
	Style.WindowMenuButtonPosition = ImGuiDir_Right;

	// Setup Platform/Renderer backends
	if (!ImGui_ImplWin32_Init(hwndAppFramework))
	{
		LOG_CONSOLE("ImGui_ImplWin32_Init.");
		return false;
	}
	if (!ImGui_ImplDX11_Init(Device, DeviceContext))
	{
		LOG_CONSOLE("ImGui_ImplDX11_Init.");
		return false;
	}

	return true;
}

void FAppFramework::ShutdownGUI()
{
	if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().BackendRendererUserData != nullptr)
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
	}

	ImGui::LogFinish();
	ImGui::DestroyContext();
}
