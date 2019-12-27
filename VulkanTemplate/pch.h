#pragma once

#include <windows.h>
#include <assert.h>

#define JASSERT(x) assert(x)
#define JMESSAGE(x) MessageBoxA(0, x, "", MB_OK)

#define FORCEINLINE __forceinline

using int8 = char;
using uint8 = unsigned char;
using int16 = short;
using uint16 = unsigned short;
using int32 = int;
using uint32 = unsigned int;
using int64 = __int64;
using uint64 = unsigned __int64;
using tchar = wchar_t;