#pragma once

#ifdef SPT_PLATFORM_WINDOWS
	#ifdef SCULPTORLIB_BUILD_DLL
		#define SCULPTORLIB_API __declspec(dllexport)
	#else
		#define SCULPTORLIB_API __declspec(dllimport)
	#endif
#else
	#error Sculptor only supports Windows
#endif // SPT_PLATFORM_WINDOWS

#pragma warning(disable : 4251) // 'Class' needs to have dll-interface to be used by clients of class
							    // Disabled as it generates lots of warnings when using header-only libraries