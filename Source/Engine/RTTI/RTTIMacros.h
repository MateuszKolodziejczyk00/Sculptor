#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef RTTI_AS_DLL
		#ifdef RTTI_BUILD_DLL
			#define RTTI_API __declspec(dllexport)
		#else
			#define RTTI_API __declspec(dllimport)
		#endif
	#else
		#define RTTI_API
	#endif // RTTI_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
