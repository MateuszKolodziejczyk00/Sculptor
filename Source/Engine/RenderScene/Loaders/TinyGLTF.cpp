#define TINYGLTF_IMPLEMENTATION

#if SPT_BUILD_LIBS_AS_DLL // If engine libs are build as static, stb is already defined in texture loader code
	#define STB_IMAGE_IMPLEMENTATION
#endif // SPT_BUILD_LIBS_AS_DLL

#include "TinyGLTF.h"

