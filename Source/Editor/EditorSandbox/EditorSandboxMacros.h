#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef EDITORSANDBOX_AS_DLL
		#ifdef EDITORSANDBOX_BUILD_DLL
			#define EDITOR_SANDBOX_API __declspec(dllexport)
		#else
			#define EDITOR_SANDBOX_API __declspec(dllimport)
		#endif
	#else
		#define EDITOR_SANDBOX_API
	#endif // EDITORSANDBOX_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
