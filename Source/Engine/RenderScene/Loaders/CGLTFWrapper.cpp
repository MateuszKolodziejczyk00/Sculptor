#if SPT_BUILD_LIBS_AS_DLL // If engine libs are build as static, stb is already defined in texture loader code
	#pragma warning(push)
	#pragma warning(disable: 4244)
	#pragma warning(disable: 4996)
	#define STB_IMAGE_IMPLEMENTATION
	#include "stb_image.h"
	#pragma warning(pop)
#endif // SPT_BUILD_LIBS_AS_DLL

#define CGLTF_IMPLEMENTATION

#pragma warning(push)
#pragma warning(disable: 4996)
#pragma warning(disable: 4100)
#pragma warning(disable: 4127)
#pragma warning(disable: 4189)
#pragma warning(disable: 4244)
#pragma warning(disable: 4267)
#pragma warning(disable: 4456)
#pragma warning(disable: 4701)
#pragma warning(disable: 4702)
#pragma warning(disable: 4706)
#include "cgltf.h"
#pragma warning(pop)

#undef CGLTF_IMPLEMENTATION
