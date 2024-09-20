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
