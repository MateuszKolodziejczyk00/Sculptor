#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHISynchronizationTypes.h"


namespace spt::vulkan
{

class RHITexture;
class RHIBuffer;
class RHICommandBuffer;
class RHIEvent;


class RHI_API RHIDependency
{
public:

	RHIDependency();

	// Building ===============================================================

	Bool		IsEmpty() const;

	SizeType	AddTextureDependency(const RHITexture& texture, const rhi::TextureSubresourceRange& subresourceRange);

	void		SetLayoutTransition(SizeType textureBarrierIdx, const rhi::BarrierTextureTransitionDefinition& transitionTarget);
	void		SetLayoutTransition(SizeType textureBarrierIdx, const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget);

	SizeType	AddBufferDependency(const RHIBuffer& buffer, SizeType offset, SizeType size);
	void		SetBufferDependencyStages(SizeType bufferIdx, rhi::EPipelineStage destStage, rhi::EAccessType destAccess);
	void		SetBufferDependencyStages(SizeType bufferIdx, rhi::EPipelineStage sourceStage, rhi::EAccessType sourceAccess, rhi::EPipelineStage destStage, rhi::EAccessType destAccess);

	// Execution ==============================================================

	void ExecuteBarrier(const RHICommandBuffer& cmdBuffer);

	void SetEvent(const RHICommandBuffer& cmdBuffer, const RHIEvent& event);
	void WaitEvent(const RHICommandBuffer& cmdBuffer, const RHIEvent& event);

	// Vulkan Only ============================================================

	VkDependencyInfo GetDependencyInfo() const;
	
private:

	void ValidateBarriers() const;

	lib::DynamicArray<VkImageMemoryBarrier2>	m_textureBarriers;
	lib::DynamicArray<VkBufferMemoryBarrier2>	m_bufferBarriers;
};

} // spt::vulkan
