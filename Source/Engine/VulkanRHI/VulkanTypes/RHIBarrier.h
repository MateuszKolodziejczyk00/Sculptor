#pragma once

#include "Vulkan.h"
#include "SculptorCoreTypes.h"
#include "RHITextureTypes.h"


namespace spt::vulkan
{

class RHITexture;
class RHICommandBuffer;


class RHIBarrier
{
public:

	RHIBarrier();

	SizeType									AddTextureBarrier(const RHITexture& texture, rhi::TextureSubresourceRange& subresourceRange);

	void										SetLayoutTransition(SizeType barrierIdx, const rhi::BarrierTextureTransitionTarget& transitionTarget);

	void										Execute(const RHICommandBuffer& cmdBuffer);

private:

	void										PrepareLayoutTransitionsForCommandBuffer(const RHICommandBuffer& cmdBuffer);
	void										WriteNewLayoutsToLayoutsManager(const RHICommandBuffer& cmdBuffer);

	lib::DynamicArray<VkImageMemoryBarrier2>	m_textureBarriers;
	lib::DynamicArray<VkBufferMemoryBarrier2>	m_bufferBarriers;
};

}
