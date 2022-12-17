#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHISynchronizationTypes.h"


namespace spt::vulkan
{

class RHITexture;
class RHICommandBuffer;
class RHIEvent;


class RHI_API RHIDependency
{
public:

	RHIDependency();

	// Building ===============================================================

	Bool		IsEmpty() const;

	SizeType	AddTextureDependency(const RHITexture& texture, const rhi::TextureSubresourceRange& subresourceRange);

	void		SetLayoutTransition(SizeType barrierIdx, const rhi::BarrierTextureTransitionDefinition& transitionTarget);
	void		SetLayoutTransition(SizeType barrierIdx, const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget);

	// Execution ==============================================================

	void ExecuteBarrier(const RHICommandBuffer& cmdBuffer);

	void SetEvent(const RHICommandBuffer& cmdBuffer, const RHIEvent& event);
	void WaitEvent(const RHICommandBuffer& cmdBuffer, const RHIEvent& event);

	// Vulkan Only ============================================================

	VkDependencyInfo GetDependencyInfo() const;
	
private:

	void PrepareLayoutTransitionsForCommandBuffer(const RHICommandBuffer& cmdBuffer);
	void WriteNewLayoutsToLayoutsManager(const RHICommandBuffer& cmdBuffer);

	void ReleaseTexturesWriteAccess(const RHICommandBuffer& cmdBuffer);
	void AcquireTexturesWriteAccess(const RHICommandBuffer& cmdBuffer);

	void ValidateBarriers() const;

	lib::DynamicArray<VkImageMemoryBarrier2>	m_textureBarriers;
	lib::DynamicArray<VkBufferMemoryBarrier2>	m_bufferBarriers;
};

} // spt::vulkan
