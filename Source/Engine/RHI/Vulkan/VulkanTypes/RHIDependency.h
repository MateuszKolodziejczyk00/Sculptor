#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHISynchronizationTypes.h"


namespace spt::vulkan
{

class RHITexture;
class RHICommandBuffer;


class RHI_API RHIDependency
{
public:

	RHIDependency();

	// Building ===============================================================

	SizeType	AddTextureDependency(const RHITexture& texture, const rhi::TextureSubresourceRange& subresourceRange);

	void		SetLayoutTransition(SizeType barrierIdx, const rhi::BarrierTextureTransitionTarget& transitionTarget);
	void		SetLayoutTransition(SizeType barrierIdx, const rhi::BarrierTextureTransitionTarget& transitionSource, const rhi::BarrierTextureTransitionTarget& transitionTarget);

	// Execution ==============================================================

	void ExecuteBarrier(const RHICommandBuffer& cmdBuffer);

	//void SetEvent()
	//void WaitEvent()

	// Vulkan Only ============================================================

	VkDependencyInfo GetDependencyInfo() const;
	
private:

	void PrepareLayoutTransitionsForCommandBuffer(const RHICommandBuffer& cmdBuffer);
	void WriteNewLayoutsToLayoutsManager(const RHICommandBuffer& cmdBuffer);

	void ValidateBarriers() const;

	lib::DynamicArray<VkImageMemoryBarrier2>	m_textureBarriers;
	lib::DynamicArray<VkBufferMemoryBarrier2>	m_bufferBarriers;
};

} // spt::vulkan
