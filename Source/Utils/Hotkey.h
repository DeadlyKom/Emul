#pragma once

#include <CoreMinimal.h>

struct FHotKey
{
	ImGuiKeyChord KeyChord;
	ImGuiInputFlags Flags;
	std::function<void()> Callback;
};

namespace HotKey
{
	static void Handler(std::vector<FHotKey> Hotkeys)
	{
		for (const FHotKey& HotKey : Hotkeys)
		{
			if (ImGui::IsKeyChordPressed(HotKey.KeyChord, HotKey.Flags)) { HotKey.Callback(); }
		}
	}
}

namespace Shortcut
{
	static void Handler(std::vector<FHotKey> Hotkeys)
	{
		for (const FHotKey& HotKey : Hotkeys)
		{
			if (ImGui::Shortcut(HotKey.KeyChord, HotKey.Flags)) { HotKey.Callback(); }
		}
	}
}
