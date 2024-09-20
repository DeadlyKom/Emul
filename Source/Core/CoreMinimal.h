#pragma once

#define BUILD_MAJOR 0
#define BUILD_MINOR 1
#define INDEX_NONE (-1)

#define WIN32_LEAN_AND_MEAN
#define IMGUI_DEFINE_MATH_OPERATORS
#pragma warning(disable : 4996)				//_CRT_SECURE_NO_WARNINGS

#include <map>
#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <functional>
#include <filesystem>

#include <d3d11.h>
#include <d3dcompiler.h>

#include <stdint.h>
#include <windows.h>

#include "imgui.h"
#include "imgui_internal.h"

#include "Utils/Log.h"
#include "Utils/Name.h"
#include "Utils/Char.h"
#include "Utils/Math_.h"
#include "Utils/Delegates.h"

namespace Debugger
{
	static const std::string Name = std::format(TEXT("ZX-Debugger ver. {}.{}"), BUILD_MAJOR, BUILD_MINOR);
}

