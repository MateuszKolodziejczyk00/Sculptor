#pragma once

#define SPT_VK_CHECK(Function) { const VkResult result = Function; SPT_CHECK(result == VK_SUCCESS); }
