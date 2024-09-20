#pragma once

struct FFrameworkFlags;

namespace Utils
{
	template<typename T>
	const FFrameworkFlags& GetFrameworkFlags()
	{
		return T::Get().GetFlags();
	}
}

#ifndef IMGUI_DISABLE_LOG
#define LOG(...)						{ if (Utils::GetFrameworkFlags().bLog) { ImGui::LogText(__VA_ARGS__); ImGui::LogText(IM_NEWLINE); } }
#define LOG_DISPLAY(...)				{ if (Utils::GetFrameworkFlags().bLog) { ImGui::LogText("Display: "); ImGui::LogText(__VA_ARGS__); ImGui::LogText(IM_NEWLINE); } }
#define LOG_WARNING(...)				{ if (Utils::GetFrameworkFlags().bLog) { ImGui::LogText("Warning: "); ImGui::LogText(__VA_ARGS__); ImGui::LogText(IM_NEWLINE); } }
#define LOG_ERROR(...)					{ if (Utils::GetFrameworkFlags().bLog) { ImGui::LogText("Error: "); ImGui::LogText(__VA_ARGS__); ImGui::LogText(IM_NEWLINE); } }
#else
#define LOG(...)
#define LOG_DISPLAY(...)
#define LOG_WARNING(...)
#define LOG_ERROR(...)	
#endif
