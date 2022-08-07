#include "VulkanCore.h"


// VMA Integration
#define VMA_IMPLEMENTATION 1
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#include "vk_mem_alloc.h"


// Volk Integration
#define VOLK_IMPLEMENTATION 1
#include "volk.h"


#undef VMA_IMPLEMENTATION
#undef VMA_STATIC_VULKAN_FUNCTIONS
#undef VMA_DYNAMIC_VULKAN_FUNCTIONS
#undef VOLK_IMPLEMENTATION
