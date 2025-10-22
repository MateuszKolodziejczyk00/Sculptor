#pragma once

#include "SculptorCore.h"

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef SHADERSTRUCTS_AS_DLL
		#ifdef SHADERSTRUCTS_BUILD_DLL
			#define SHADER_STRUCTS_API __declspec(dllexport)
		#else
			#define SHADER_STRUCTS_API __declspec(dllimport)
		#endif
	#else
		#define SHADER_STRUCTS_API
	#endif // SHADERSTRUCTS_AS_DLL
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS
