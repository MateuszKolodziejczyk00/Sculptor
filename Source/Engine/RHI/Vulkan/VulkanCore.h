#pragma once

#pragma warning(push)
#pragma warning(disable: 4100)
#pragma warning(disable: 4127)
#pragma warning(disable: 4189)
#pragma warning(disable: 4459)
#pragma warning(disable: 4324)


#define SPT_VMA_DEBUG 0

#if SPT_VMA_DEBUG

#define VMA_DEBUG_LOG_FORMAT(format, ...) printf(format, __VA_ARGS__)

#endif // RHI_DEBUG

#include "volk.h"
#include "vk_mem_alloc.h"
#include "VulkanNames.h"
#include "VulkanMacros.h"
#include "VulkanAliases.h"

#pragma warning(pop)
