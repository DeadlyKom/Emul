#pragma once

#define INDEX_NONE (-1)
#define BUFFER_SIZE_INPUT 32

#define WIN32_LEAN_AND_MEAN
#define IMGUI_DEFINE_MATH_OPERATORS
#pragma warning(disable : 4996)				//_CRT_SECURE_NO_WARNINGS

#include <any>
#include <map>
#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <ranges>
#include <chrono>
#include <variant>
#include <fstream>
#include <iostream>
#include <typeindex>
#include <functional>
#include <filesystem>
#include <unordered_map>

#include <d3d11.h>
#include <d3dcompiler.h>

#include <stdint.h>
#include <windows.h>

#include "imgui.h"
#include "imgui_internal.h"

#include "Transform.h"
#include "Utils/Log.h"
#include "Utils/Name.h"
#include "Utils/Char.h"
#include "Utils/Math_.h"
#include "Utils/Delegates.h"
#include "Utils/ScopeExit.h"

constexpr double operator""_MHz(long double Frequency)
{
	return Frequency * 1'000'000.0;
}

extern struct FFrameworkConfig
{
	bool bLog = false;
	bool bVsync = false;
	bool bFullscreen = false;
	bool bDontAskMeNextTime_Quit = false;

	uint32_t WindowWidth = INDEX_NONE;
	uint32_t WindowHeight = INDEX_NONE;

	int32_t SampleRateCapacity = 512;	// Oscillogram Manager

	std::string Application;
} FrameworkConfig;

namespace EApplication
{
	enum Type : int32_t
	{
		None = -1,
		Debugger = 0,
		Sprite,
	};
}

extern struct FFrameworkCallback
{
	int32_t Secection = EApplication::None;
} FrameworkCallback;
