#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef IESPROFILEASSET_AS_DLL
		#ifdef IESPROFILEASSET_BUILD_DLL
			#define IES_PROFILE_ASSET_API __declspec(dllexport)
		#else
			#define IES_PROFILE_ASSET_API __declspec(dllimport)
		#endif
	#else
		#define IES_PROFILE_ASSET_API
	#endif // IESPROFILEASSET_BUILD_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
