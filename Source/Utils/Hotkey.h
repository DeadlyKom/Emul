#pragma once

#include <CoreMinimal.h>

namespace HotKey
{
	static void Handler(std::map<ImGuiKeyChord, std::function<void()>> Hotkeys)
	{
		for (auto& [KeyChord, Callback] : Hotkeys)
		{
			if (ImGui::IsKeyChordPressed(KeyChord)) Callback();
		}
	}
}