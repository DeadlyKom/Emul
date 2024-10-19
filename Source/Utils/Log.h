#pragma once

#include <mutex>
#include <format>
#include <iostream>

namespace LogVerbosity
{
	enum Type
	{
		NoLogging,
		Fatal,
		Error,
		Warning,
		Display,
		Log,
		Verbose,
		VeryVerbose,
	};
}

namespace Utils
{	

	#define CONSOLE_BLACK	(0)
	#define CONSOLE_BLUE	(FOREGROUND_BLUE)
	#define CONSOLE_RED		(FOREGROUND_RED)
	#define CONSOLE_GREEN	(FOREGROUND_BLUE | FOREGROUND_GREEN)
	#define CONSOLE_MAGENTA	(FOREGROUND_RED)
	#define CONSOLE_YELLOW	(FOREGROUND_RED | FOREGROUND_GREEN)
	#define CONSOLE_CYAN	(FOREGROUND_BLUE | FOREGROUND_GREEN)
	#define CONSOLE_WHITE	(FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN)

	inline void SetConsoleColor(int32_t TextColor)
	{
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hConsole, TextColor);
	}

	template <typename... Args>
	void Log(LogVerbosity::Type Verbosity, const std::string& _fmt, Args&&... _Args)
	{
		static std::mutex LogMutex;

		LogMutex.lock();
		switch (Verbosity)
		{
		case LogVerbosity::Error:
			SetConsoleColor(CONSOLE_RED);
			break;
		case LogVerbosity::Warning:
			SetConsoleColor(CONSOLE_BLUE);
			break;
		case LogVerbosity::Display:
			SetConsoleColor(CONSOLE_YELLOW);
			break;
		case LogVerbosity::Log:
			SetConsoleColor(CONSOLE_WHITE);
			break;
		default:
			SetConsoleColor(CONSOLE_CYAN);
			break;
		}
		std::cout << std::vformat(_fmt, std::make_format_args(_Args...)) << std::endl;
		SetConsoleColor(CONSOLE_WHITE);
		LogMutex.unlock();
	}
}

#ifndef IMGUI_DISABLE_LOG
#define LOG(...)						{ if (FrameworkFlags.bLog) { Utils::Log(LogVerbosity::Log, __VA_ARGS__); } }
#define LOG_DISPLAY(...)				{ if (FrameworkFlags.bLog) { Utils::Log(LogVerbosity::Display, __VA_ARGS__); } }
#define LOG_WARNING(...)				{ if (FrameworkFlags.bLog) { Utils::Log(LogVerbosity::Warning, __VA_ARGS__); } }
#define LOG_ERROR(...)					{ if (FrameworkFlags.bLog) { Utils::Log(LogVerbosity::Error, __VA_ARGS__);} }
#else
#define LOG(...)
#define LOG_DISPLAY(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)	
#endif

