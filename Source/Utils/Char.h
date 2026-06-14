#pragma once

// An ANSI character. 8-bit fixed-width representation of 7-bit characters.
typedef char				ANSICHAR;
// A wide character. In-memory only. ?-bit fixed-width representation of the platform's natural wide character set. Could be different sizes on different platforms.
typedef wchar_t				WIDECHAR;
// A switchable character. In-memory only. Either ANSICHAR or WIDECHAR, depending on a licensee's requirements.
//typedef WIDECHAR			TCHAR;

// Usage of these should be replaced with StringCasts.
#define TCHAR_TO_ANSI(str) (ANSICHAR*)static_cast<ANSICHAR>(Utils::Utf16ToUtf8(static_cast<const TCHAR*>(str)).c_str()
#define ANSI_TO_TCHAR(str) (TCHAR*)static_cast<TCHAR>(Utils::Utf8ToUtf16(static_cast<const ANSICHAR*>(str)).c_str()

namespace Utils
{
	inline std::string Utf16ToUtf8(const std::wstring& Text)
	{
		if (Text.empty())
		{
			return {};
		}

		const int Size = WideCharToMultiByte(
			CP_UTF8,
			0,
			Text.data(),
			(int)Text.size(),
			nullptr,
			0,
			nullptr,
			nullptr
		);

		std::string Result;
		Result.resize(Size);

		WideCharToMultiByte(
			CP_UTF8,
			0,
			Text.data(),
			(int)Text.size(),
			Result.data(),
			Size,
			nullptr,
			nullptr
		);

		return Result;
	}

	inline std::wstring Utf8ToUtf16(const std::string& Text)
	{
		if (Text.empty())
		{
			return {};
		}

		const int Size = MultiByteToWideChar(
			CP_UTF8,
			0,
			Text.data(),
			(int)Text.size(),
			nullptr,
			0
		);

		std::wstring Result;
		Result.resize(Size);

		MultiByteToWideChar(
			CP_UTF8,
			0,
			Text.data(),
			(int)Text.size(),
			Result.data(),
			Size
		);

		return Result;
	}

	inline void CopyToBuffer(char* Buffer, size_t BufferSize, const std::string& Text)
	{
		if (Buffer == nullptr || BufferSize == 0)
		{
			return;
		}

		snprintf(Buffer, BufferSize, "%s", Text.c_str());
	}
}
