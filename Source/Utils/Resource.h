#pragma once

#include <CoreMinimal.h>

namespace Resource
{
	static inline bool Get(std::vector<uint8_t>& OutData, WORD ResourceID, const std::string& Folder = "")
	{
		HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
		HRSRC hResource = FindResource(hInstance, MAKEINTRESOURCE(ResourceID), Folder.c_str());
		HGLOBAL hLoadedResource = hResource != NULL ? LoadResource(hInstance, hResource) : NULL;
		LPVOID lpResourceLock = hLoadedResource != NULL ? LockResource(hLoadedResource) : NULL;
		if (lpResourceLock == NULL)
		{
			return false;
		}

		DWORD Size = SizeofResource(hInstance, hResource);
		const uint8_t* Data = reinterpret_cast<const uint8_t*>(LockResource(hLoadedResource));

		// reserve capacity
		OutData.clear();
		OutData.resize(Size);
		std::memcpy(OutData.data(), Data, Size);

		FreeResource(hLoadedResource);
		return true;
	}
}
