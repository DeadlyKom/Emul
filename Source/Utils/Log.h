#pragma once

#include <mutex>
#include <format>
#include <iostream>

class FAppFramework;

namespace Utils
{	
	template <typename... Args>
	constexpr void Log(const std::string& _fmt, Args&&... _Args)
	{
		static std::mutex LogMutex;

		LogMutex.lock();
		std::cout << std::vformat(_fmt, std::make_format_args(_Args...)) << std::endl;
		LogMutex.unlock();
	}
}

#ifndef IMGUI_DISABLE_LOG
#define LOG(...)						{ if (FAppFramework::Flags.bLog) { ImGui::LogText(__VA_ARGS__); ImGui::LogText(IM_NEWLINE); } }
#define LOG_CONSOLE(...)				{ if (FAppFramework::Flags.bLog) { Utils::Log(__VA_ARGS__); } }
#define LOG_DISPLAY(...)				{ if (FAppFramework::Flags.bLog) { ImGui::LogText("Display: "); ImGui::LogText(__VA_ARGS__); ImGui::LogText(IM_NEWLINE); } }
#define LOG_WARNING(...)				{ if (FAppFramework::Flags.bLog) { ImGui::LogText("Warning: "); ImGui::LogText(__VA_ARGS__); ImGui::LogText(IM_NEWLINE); } }
#define LOG_ERROR(...)					{ if (FAppFramework::Flags.bLog) { ImGui::LogText("Error: "); ImGui::LogText(__VA_ARGS__); ImGui::LogText(IM_NEWLINE); } }
#else
#define LOG(...)
#define LOG_DISPLAY(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)	
#endif

