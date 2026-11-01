#pragma once

#ifndef SPT_PLATFORM_WINDOWS
#error "SculptorLibWindows.h can be included only on Windows"
#endif // SPT_PLATFORM_WINDOWS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
