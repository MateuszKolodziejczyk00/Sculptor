#pragma once

#include "Vulkan.h"
#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

class RHIBarrier
{
public:

	RHIBarrier();

	//void AddTextureBarrier();

private:

	lib::DynamicArray<VkImageMemoryBarrier2>	m_textureBarriers;
	lib::DynamicArray<VkBufferMemoryBarrier2>	m_bufferBarriers;
};

}
